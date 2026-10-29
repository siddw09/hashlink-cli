#ifndef FILE_STORAGE_H
#define FILE_STORAGE_H

#include "URLRecord.h"
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

/**
 * @brief Result struct returned by load operation.
 */
struct StorageLoadResult {
    bool success{false};
    std::string errorMessage{""};
    uint64_t nextId{1};
    std::vector<URLRecord> records{};
    size_t corruptedLineCount{0};
};

/**
 * @brief Handles file-based persistence for HashLink CLI.
 * 
 * Storage format:
 * Line 1: Header metadata -> `# HASHLINK_STORAGE_V1 next_id=<uint64_t>`
 * Subsequent lines: `<shortCode> <quoted_originalURL> <epochSeconds>`
 * Uses std::quoted to robustly handle spaces, queries, and special characters.
 */
class FileStorage {
private:
    std::filesystem::path filePath_;

public:
    /**
     * @brief Constructs FileStorage targeting a specific file path.
     * @param filePath Path to storage file (default: "./data/hashlink.db").
     */
    explicit FileStorage(std::filesystem::path filePath = "data/hashlink.db");

    /**
     * @brief Loads records and the next ID from persistent disk storage.
     * @return StorageLoadResult with loaded records, nextId, and status.
     */
    StorageLoadResult load();

    /**
     * @brief Saves all active records and next ID to disk atomically.
     * Writes to a temporary file first, then renames to avoid corruption on crash.
     * @param records Collection of active URL records to persist.
     * @param nextId Next available numeric ID for Base62 encoding.
     * @return true on success, false on I/O error.
     */
    bool save(const std::vector<URLRecord>& records, uint64_t nextId);

    /**
     * @brief Returns the configured file path.
     */
    const std::filesystem::path& getFilePath() const noexcept;

    /**
     * @brief Checks if the database file currently exists on disk.
     */
    bool exists() const;
};

#endif // FILE_STORAGE_H
