#include "URLShortener.h"
#include <algorithm>
#include <cctype>
#include <iostream>

static const std::string BASE62_CHARS =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

URLShortener::URLShortener(const std::string& storagePath, size_t initialCapacity)
    : hashTable_(initialCapacity),
      storage_(storagePath),
      nextId_(1),
      storageCorrupted_(false) {}

std::string URLShortener::encodeBase62(uint64_t id) {
    if (id == 0) {
        return "0";
    }

    std::string result;
    while (id > 0) {
        result.push_back(BASE62_CHARS[id % 62]);
        id /= 62;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

uint64_t URLShortener::decodeBase62(const std::string& str) {
    uint64_t result = 0;
    for (char c : str) {
        size_t index = BASE62_CHARS.find(c);
        if (index == std::string::npos) {
            return 0; // Malformed character
        }
        result = result * 62 + index;
    }
    return result;
}

bool URLShortener::validateURL(const std::string& url, std::string& failureReason) {
    if (url.empty()) {
        failureReason = "URL cannot be empty.";
        return false;
    }

    // Reject whitespace or control characters
    for (char c : url) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            failureReason = "URL cannot contain spaces or whitespace characters.";
            return false;
        }
    }

    // Check scheme: must start with http:// or https://
    const std::string httpPrefix = "http://";
    const std::string httpsPrefix = "https://";

    size_t schemeLen = 0;
    if (url.rfind(httpsPrefix, 0) == 0) {
        schemeLen = httpsPrefix.length();
    } else if (url.rfind(httpPrefix, 0) == 0) {
        schemeLen = httpPrefix.length();
    } else {
        failureReason = "Invalid URL scheme. Must begin with 'http://' or 'https://'.";
        return false;
    }

    // Host portion validation: host must not be empty or immediately start with invalid characters
    if (url.length() <= schemeLen) {
        failureReason = "URL is missing a host domain (e.g., https://example.com).";
        return false;
    }

    char firstHostChar = url[schemeLen];
    if (firstHostChar == '/' || firstHostChar == ':' || firstHostChar == '?') {
        failureReason = "Malformed URL host segment.";
        return false;
    }

    return true;
}

ShortenResult URLShortener::shortenURL(const std::string& originalURL) {
    ShortenResult res;

    // Step 1: Validate URL
    std::string validationError;
    if (!validateURL(originalURL, validationError)) {
        res.success = false;
        res.message = "Validation Failed: " + validationError;
        return res;
    }

    // Step 2: Linear scan to check for duplicate original URL
    // Complexity: O(N) where N is number of active records.
    // For educational mini-projects, this avoids maintaining two synchronized hash tables.
    auto activeRecords = hashTable_.getAllRecords();
    for (const auto& rec : activeRecords) {
        if (rec.getOriginalURL() == originalURL) {
            res.success = true;
            res.isDuplicate = true;
            res.shortCode = rec.getShortCode();
            res.message = "URL already shortened. Returning existing short code: " + rec.getShortCode();
            return res;
        }
    }

    // Step 3: Generate unique short code using monotonic numeric ID encoded in Base62
    std::string code;
    do {
        code = encodeBase62(nextId_++);
    } while (hashTable_.contains(code));

    // Step 4: Insert into custom Hash Table
    URLRecord newRecord(code, originalURL);
    if (!hashTable_.insert(newRecord)) {
        res.success = false;
        res.message = "Internal Error: Hash table rejected record insertion.";
        return res;
    }

    // Step 5: Persist mutation if storage file is healthy
    if (!storageCorrupted_) {
        saveToDisk();
    }

    res.success = true;
    res.isDuplicate = false;
    res.shortCode = code;
    res.message = "Successfully shortened URL. Generated code: " + code;
    return res;
}

std::optional<std::string> URLShortener::getOriginalURL(const std::string& shortCode) const {
    const URLRecord* rec = hashTable_.find(shortCode);
    if (rec != nullptr) {
        return rec->getOriginalURL();
    }
    return std::nullopt;
}

std::vector<URLRecord> URLShortener::listURLs() const {
    return hashTable_.getAllRecords();
}

bool URLShortener::deleteURL(const std::string& shortCode) {
    bool removed = hashTable_.remove(shortCode);
    if (removed && !storageCorrupted_) {
        // Save updated records without decrementing nextId_
        saveToDisk();
    }
    return removed;
}

ShortenerStats URLShortener::getStatistics() const {
    ShortenerStats stats;
    stats.activeRecords = hashTable_.size();
    stats.capacity = hashTable_.capacity();
    stats.loadFactor = hashTable_.loadFactor();
    stats.maxLoadFactor = hashTable_.maxLoadFactor();
    stats.collisionEvents = hashTable_.getCollisionCount();
    stats.totalProbeSteps = hashTable_.getTotalProbes();
    stats.resizeCount = hashTable_.getResizeCount();
    stats.tombstoneCount = hashTable_.tombstoneCount();
    stats.nextId = nextId_;
    stats.dbPath = storage_.getFilePath().string();
    return stats;
}

bool URLShortener::loadFromDisk(std::string& statusMessage) {
    if (!storage_.exists()) {
        statusMessage = "No existing database found. Starting with a fresh session.";
        return true;
    }

    StorageLoadResult loadRes = storage_.load();
    if (!loadRes.success) {
        storageCorrupted_ = true;
        statusMessage = "CRITICAL STORAGE WARNING: " + loadRes.errorMessage +
                        "\nTo protect data integrity, auto-save is DISABLED until resolved.";
        return false;
    }

    hashTable_.clear();
    uint64_t highestSeenId = 0;

    for (const auto& rec : loadRes.records) {
        hashTable_.insert(rec);
        uint64_t decoded = decodeBase62(rec.getShortCode());
        if (decoded > highestSeenId) {
            highestSeenId = decoded;
        }
    }

    // Guarantee nextId_ is strictly greater than both loaded next_id and all existing short code IDs
    nextId_ = std::max(loadRes.nextId, highestSeenId + 1);
    storageCorrupted_ = false;
    statusMessage = "Successfully loaded " + std::to_string(loadRes.records.size()) +
                    " record(s) from " + storage_.getFilePath().string();
    return true;
}

bool URLShortener::saveToDisk() {
    if (storageCorrupted_) {
        std::cerr << "[WARNING] Save aborted to prevent overwriting corrupted database." << std::endl;
        return false;
    }
    return storage_.save(hashTable_.getAllRecords(), nextId_);
}

bool URLShortener::isStorageCorrupted() const noexcept {
    return storageCorrupted_;
}

const HashTable& URLShortener::getHashTable() const noexcept {
    return hashTable_;
}

uint64_t URLShortener::getNextId() const noexcept {
    return nextId_;
}
