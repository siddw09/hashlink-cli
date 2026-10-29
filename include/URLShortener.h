#ifndef URL_SHORTENER_H
#define URL_SHORTENER_H

#include "HashTable.h"
#include "FileStorage.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

/**
 * @brief Aggregated runtime statistics for HashLink.
 */
struct ShortenerStats {
    size_t activeRecords{0};
    size_t capacity{0};
    double loadFactor{0.0};
    double maxLoadFactor{0.0};
    size_t collisionEvents{0};
    size_t totalProbeSteps{0};
    size_t resizeCount{0};
    size_t tombstoneCount{0};
    uint64_t nextId{1};
    std::string dbPath{""};
};

/**
 * @brief Result of a shorten operation.
 */
struct ShortenResult {
    bool success{false};
    bool isDuplicate{false}; // True if URL was already shortened previously
    std::string shortCode{""};
    std::string message{""};
};

/**
 * @brief Core business logic coordinating URL shortening, deduplication,
 * Base62 ID encoding, and persistent storage.
 */
class URLShortener {
private:
    HashTable hashTable_;
    FileStorage storage_;
    uint64_t nextId_{1};
    bool storageCorrupted_{false};

public:
    /**
     * @brief Constructs URLShortener with an optional custom database path.
     * @param storagePath Path to storage file.
     * @param initialCapacity Initial table size.
     */
    explicit URLShortener(const std::string& storagePath = "data/hashlink.db",
                          size_t initialCapacity = HashTable::DEFAULT_INITIAL_CAPACITY);

    /**
     * @brief Encodes a 64-bit integer into a Base62 string deterministically.
     * Uses character set [0-9a-zA-Z].
     */
    static std::string encodeBase62(uint64_t id);

    /**
     * @brief Decodes a Base62 string back to a 64-bit integer.
     */
    static uint64_t decodeBase62(const std::string& str);

    /**
     * @brief Validates basic HTTP/HTTPS URL syntax.
     * Rejects empty URLs, whitespace, and unsupported schemes.
     * @param url Input string.
     * @param failureReason Output explanation if invalid.
     * @return true if valid, false otherwise.
     */
    static bool validateURL(const std::string& url, std::string& failureReason);

    /**
     * @brief Shortens a long URL.
     * If the URL has already been shortened, returns the existing short code.
     * Otherwise generates a new unique short code via Base62 and inserts it.
     */
    ShortenResult shortenURL(const std::string& originalURL);

    /**
     * @brief Retrieves the original URL given a short code.
     * @return std::optional containing the URL if found, nullopt otherwise.
     */
    std::optional<std::string> getOriginalURL(const std::string& shortCode) const;

    /**
     * @brief Returns a copy of all active URL records.
     */
    std::vector<URLRecord> listURLs() const;

    /**
     * @brief Deletes a shortened URL record.
     * Monotonic nextId is preserved to avoid key reuse.
     * @return true if deleted, false if not found.
     */
    bool deleteURL(const std::string& shortCode);

    /**
     * @brief Returns comprehensive hash table and application statistics.
     */
    ShortenerStats getStatistics() const;

    /**
     * @brief Loads records from disk. Called during startup.
     * @return true if loaded cleanly, false if file corrupted or I/O failure.
     */
    bool loadFromDisk(std::string& statusMessage);

    /**
     * @brief Persists all active records to disk.
     */
    bool saveToDisk();

    /**
     * @brief Returns whether database loading encountered corruption.
     */
    bool isStorageCorrupted() const noexcept;

    /**
     * @brief Access internal hash table directly (primarily for testing and metrics).
     */
    const HashTable& getHashTable() const noexcept;
    uint64_t getNextId() const noexcept;
};

#endif // URL_SHORTENER_H
