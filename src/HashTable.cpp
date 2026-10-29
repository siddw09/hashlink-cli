#include "HashTable.h"
#include <algorithm>
#include <stdexcept>

HashTable::HashTable(size_t initialCapacity, double maxLoadFactor, HashFunc customHash)
    : capacity_(findNextPrime(std::max<size_t>(initialCapacity, 5))),
      maxLoadFactor_(maxLoadFactor),
      customHashFunc_(std::move(customHash)) {
    table_.resize(capacity_);
}

bool HashTable::isPrime(size_t n) noexcept {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (size_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

size_t HashTable::findNextPrime(size_t n) {
    if (n <= 2) return 2;
    size_t prime = (n % 2 == 0) ? n + 1 : n;
    while (!isPrime(prime)) {
        prime += 2;
    }
    return prime;
}

size_t HashTable::defaultPolynomialHash(const std::string& key, size_t cap) const noexcept {
    // Polynomial rolling hash with prime multiplier p = 53
    // hash = (s[0]*p^(k-1) + s[1]*p^(k-2) + ... + s[k-1]) % cap
    // Implemented via Horner's rule to compute in O(k) without large exponents.
    const size_t p = 53;
    size_t hashValue = 0;
    for (unsigned char c : key) {
        hashValue = (hashValue * p + c) % cap;
    }
    return hashValue;
}

size_t HashTable::computeHash(const std::string& key, size_t cap) const {
    if (customHashFunc_) {
        return customHashFunc_(key, cap) % cap;
    }
    return defaultPolynomialHash(key, cap);
}

void HashTable::resize(size_t newCapacity) {
    size_t safeNewCap = findNextPrime(std::max(newCapacity, capacity_ + 1));
    std::vector<HashSlot> oldTable = std::move(table_);

    table_.assign(safeNewCap, HashSlot{});
    capacity_ = safeNewCap;
    activeCount_ = 0;
    deletedCount_ = 0;
    resizeCount_++;

    // Rehash all OCCUPIED records into the newly sized table.
    // DELETED slots (tombstones) are naturally purged during rehashing.
    for (const auto& slot : oldTable) {
        if (slot.state == SlotState::OCCUPIED) {
            insert(slot.record);
        }
    }
}

bool HashTable::insert(const URLRecord& record) {
    const std::string& key = record.getShortCode();
    if (key.empty()) {
        return false;
    }

    // Check load factor threshold: alpha = (active + 1) / capacity
    // Also rehash if tombstones saturate the table, preventing long probe chains
    if (static_cast<double>(activeCount_ + 1) / static_cast<double>(capacity_) > maxLoadFactor_ ||
        (activeCount_ + deletedCount_ + 1) >= capacity_) {
        resize(capacity_ * 2);
    }

    const size_t initialIndex = computeHash(key, capacity_);
    int firstDeletedIndex = -1;
    bool hadCollision = false;

    // Linear probing sequence: index = (initialIndex + probe) % capacity
    for (size_t probe = 0; probe < capacity_; ++probe) {
        size_t currIndex = (initialIndex + probe) % capacity_;
        totalProbeSteps_++;

        const SlotState currentState = table_[currIndex].state;

        if (currentState == SlotState::OCCUPIED) {
            if (table_[currIndex].record.getShortCode() == key) {
                // Key already exists: do not silently overwrite
                return false;
            }
            hadCollision = true;
        } else if (currentState == SlotState::DELETED) {
            // Tombstone encountered. Note the first tombstone for insertion,
            // but continue probing to verify key is not duplicated farther along.
            if (firstDeletedIndex == -1) {
                firstDeletedIndex = static_cast<int>(currIndex);
            }
            hadCollision = true;
        } else {
            // SlotState::EMPTY: Key definitely does not exist in table
            size_t insertIndex = (firstDeletedIndex != -1) ? static_cast<size_t>(firstDeletedIndex) : currIndex;

            if (firstDeletedIndex != -1) {
                deletedCount_--;
            }

            table_[insertIndex].state = SlotState::OCCUPIED;
            table_[insertIndex].record = record;
            activeCount_++;

            if (hadCollision) {
                totalCollisionEvents_++;
            }
            return true;
        }
    }

    // If probing scanned the entire capacity (only possible if table was full of tombstones/occupied)
    if (firstDeletedIndex != -1) {
        table_[firstDeletedIndex].state = SlotState::OCCUPIED;
        table_[firstDeletedIndex].record = record;
        deletedCount_--;
        activeCount_++;
        if (hadCollision) {
            totalCollisionEvents_++;
        }
        return true;
    }

    // Fallback: should not be reached with proactive resizing, but guarantees safety
    resize(capacity_ * 2);
    return insert(record);
}

const URLRecord* HashTable::find(const std::string& shortCode) const {
    if (shortCode.empty() || activeCount_ == 0) {
        return nullptr;
    }

    const size_t initialIndex = computeHash(shortCode, capacity_);

    for (size_t probe = 0; probe < capacity_; ++probe) {
        size_t currIndex = (initialIndex + probe) % capacity_;
        // Cast away constness strictly for telemetry counter
        const_cast<HashTable*>(this)->totalProbeSteps_++;

        const SlotState state = table_[currIndex].state;

        if (state == SlotState::EMPTY) {
            // Reached empty slot: key is guaranteed not to exist
            return nullptr;
        } else if (state == SlotState::OCCUPIED) {
            if (table_[currIndex].record.getShortCode() == shortCode) {
                return &table_[currIndex].record;
            }
        }
        // If state == SlotState::DELETED (Tombstone): continue probing!
    }

    return nullptr;
}

bool HashTable::remove(const std::string& shortCode) {
    if (shortCode.empty() || activeCount_ == 0) {
        return false;
    }

    const size_t initialIndex = computeHash(shortCode, capacity_);

    for (size_t probe = 0; probe < capacity_; ++probe) {
        size_t currIndex = (initialIndex + probe) % capacity_;
        totalProbeSteps_++;

        const SlotState state = table_[currIndex].state;

        if (state == SlotState::EMPTY) {
            return false;
        } else if (state == SlotState::OCCUPIED) {
            if (table_[currIndex].record.getShortCode() == shortCode) {
                // Tombstone deletion: mark slot as DELETED without breaking probe sequences
                table_[currIndex].state = SlotState::DELETED;
                table_[currIndex].record = URLRecord{};
                activeCount_--;
                deletedCount_++;
                return true;
            }
        }
        // If state == SlotState::DELETED: continue probing
    }

    return false;
}

bool HashTable::contains(const std::string& shortCode) const {
    return find(shortCode) != nullptr;
}

void HashTable::clear() {
    table_.assign(capacity_, HashSlot{});
    activeCount_ = 0;
    deletedCount_ = 0;
    totalCollisionEvents_ = 0;
    totalProbeSteps_ = 0;
    resizeCount_ = 0;
}

size_t HashTable::size() const noexcept {
    return activeCount_;
}

size_t HashTable::capacity() const noexcept {
    return capacity_;
}

size_t HashTable::tombstoneCount() const noexcept {
    return deletedCount_;
}

double HashTable::loadFactor() const noexcept {
    if (capacity_ == 0) return 0.0;
    return static_cast<double>(activeCount_) / static_cast<double>(capacity_);
}

double HashTable::maxLoadFactor() const noexcept {
    return maxLoadFactor_;
}

size_t HashTable::getCollisionCount() const noexcept {
    return totalCollisionEvents_;
}

size_t HashTable::getTotalProbes() const noexcept {
    return totalProbeSteps_;
}

size_t HashTable::getResizeCount() const noexcept {
    return resizeCount_;
}

std::vector<URLRecord> HashTable::getAllRecords() const {
    std::vector<URLRecord> records;
    records.reserve(activeCount_);
    for (const auto& slot : table_) {
        if (slot.state == SlotState::OCCUPIED) {
            records.push_back(slot.record);
        }
    }
    return records;
}

size_t HashTable::getPrimaryHashIndex(const std::string& key) const {
    return computeHash(key, capacity_);
}

SlotState HashTable::getSlotState(size_t index) const {
    if (index >= capacity_) {
        throw std::out_of_range("HashTable::getSlotState index out of bounds");
    }
    return table_[index].state;
}

const URLRecord* HashTable::getSlotRecord(size_t index) const {
    if (index >= capacity_) {
        throw std::out_of_range("HashTable::getSlotRecord index out of bounds");
    }
    return &table_[index].record;
}
