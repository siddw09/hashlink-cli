#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "URLRecord.h"
#include <vector>
#include <string>
#include <functional>
#include <cstddef>

/**
 * @brief Represents the lifecycle state of a slot in open addressing.
 * 
 * - EMPTY: Slot has never contained any element. Terminal condition for searches.
 * - OCCUPIED: Slot holds a currently valid active record.
 * - DELETED: Tombstone marker. Previously held an element that was deleted.
 *            Searches must NOT stop at a tombstone; insertions may reuse it.
 */
enum class SlotState {
    EMPTY,
    OCCUPIED,
    DELETED
};

/**
 * @brief Internal storage bucket for HashTable.
 */
struct HashSlot {
    SlotState state{SlotState::EMPTY};
    URLRecord record{};
};

/**
 * @brief Custom Open-Addressing Hash Table using Linear Probing and Tombstones.
 * 
 * Designed to satisfy University Advanced Data Structures & Algorithms (ADSA) requirements.
 * Does not use std::unordered_map or any external hash containers.
 */
class HashTable {
public:
    using HashFunc = std::function<size_t(const std::string&, size_t)>;

    static constexpr size_t DEFAULT_INITIAL_CAPACITY = 11;
    static constexpr double DEFAULT_MAX_LOAD_FACTOR = 0.70;

private:
    std::vector<HashSlot> table_;
    size_t capacity_;
    size_t activeCount_{0};
    size_t deletedCount_{0};
    double maxLoadFactor_;
    
    // Statistics counters
    size_t totalCollisionEvents_{0}; // Count of insertions where initial bucket was occupied
    size_t totalProbeSteps_{0};       // Total probe steps performed across operations
    size_t resizeCount_{0};           // Number of dynamic resize & rehash operations performed

    // Hash function (default: polynomial rolling hash; customizable for unit tests)
    HashFunc customHashFunc_{nullptr};

    /**
     * @brief Computes polynomial rolling hash for a string key modulo capacity.
     */
    size_t defaultPolynomialHash(const std::string& key, size_t cap) const noexcept;

    /**
     * @brief Invokes the active hash function (custom if injected, otherwise polynomial).
     */
    size_t computeHash(const std::string& key, size_t cap) const;

    /**
     * @brief Finds the smallest prime number >= n (helps minimize clustering).
     */
    static size_t findNextPrime(size_t n);

    /**
     * @brief Tests whether an integer is prime.
     */
    static bool isPrime(size_t n) noexcept;

    /**
     * @brief Dynamically resizes the table and rehashes all active records.
     * Purges all DELETED tombstone slots.
     */
    void resize(size_t newCapacity);

public:
    /**
     * @brief Constructs a hash table with given initial capacity and load factor.
     */
    explicit HashTable(size_t initialCapacity = DEFAULT_INITIAL_CAPACITY,
                       double maxLoadFactor = DEFAULT_MAX_LOAD_FACTOR,
                       HashFunc customHash = nullptr);

    /**
     * @brief Inserts a new record into the hash table.
     * @param record The URLRecord to insert.
     * @return true if insertion succeeded; false if a record with the same shortCode already exists.
     */
    bool insert(const URLRecord& record);

    /**
     * @brief Searches for a record by its short code.
     * Follows the linear probe sequence across tombstones until found or an EMPTY slot is reached.
     * @param shortCode The key to find.
     * @return Pointer to URLRecord if found, nullptr otherwise.
     */
    const URLRecord* find(const std::string& shortCode) const;

    /**
     * @brief Deletes a record by short code by marking its slot as DELETED (tombstone).
     * @param shortCode The key to delete.
     * @return true if record was found and deleted; false if key did not exist.
     */
    bool remove(const std::string& shortCode);

    /**
     * @brief Checks whether a record with shortCode exists in the table.
     */
    bool contains(const std::string& shortCode) const;

    /**
     * @brief Clears all records and tombstones, resetting statistics.
     */
    void clear();

    // Capacity and Size accessors
    size_t size() const noexcept;
    size_t capacity() const noexcept;
    size_t tombstoneCount() const noexcept;
    double loadFactor() const noexcept;
    double maxLoadFactor() const noexcept;

    // Diagnostic & Instrumentation accessors
    size_t getCollisionCount() const noexcept;
    size_t getTotalProbes() const noexcept;
    size_t getResizeCount() const noexcept;

    /**
     * @brief Returns a copy of all active records in the table.
     */
    std::vector<URLRecord> getAllRecords() const;

    /**
     * @brief Exposes the primary hash index for a key (useful for verification and tests).
     */
    size_t getPrimaryHashIndex(const std::string& key) const;

    /**
     * @brief Direct slot inspection for white-box testing of probe chains and tombstones.
     */
    SlotState getSlotState(size_t index) const;
    const URLRecord* getSlotRecord(size_t index) const;
};

#endif // HASH_TABLE_H
