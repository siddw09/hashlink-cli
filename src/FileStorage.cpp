#include "FileStorage.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <unordered_set>

FileStorage::FileStorage(std::filesystem::path filePath)
    : filePath_(std::move(filePath)) {}

const std::filesystem::path& FileStorage::getFilePath() const noexcept {
    return filePath_;
}

bool FileStorage::exists() const {
    std::error_code ec;
    return std::filesystem::exists(filePath_, ec);
}

StorageLoadResult FileStorage::load() {
    StorageLoadResult result;
    result.nextId = 1;

    std::error_code ec;
    if (!std::filesystem::exists(filePath_, ec)) {
        // Normal clean startup on first run
        result.success = true;
        return result;
    }

    std::ifstream inFile(filePath_);
    if (!inFile.is_open()) {
        result.success = false;
        result.errorMessage = "Failed to open database file for reading: " + filePath_.string();
        return result;
    }

    std::string line;
    size_t lineNumber = 0;
    std::vector<std::string> seenCodes;

    while (std::getline(inFile, line)) {
        lineNumber++;
        if (line.empty()) continue;

        // Process Header
        if (line.rfind("# HASHLINK_STORAGE_V1", 0) == 0) {
            auto pos = line.find("next_id=");
            if (pos != std::string::npos) {
                try {
                    result.nextId = std::stoull(line.substr(pos + 8));
                } catch (...) {
                    result.corruptedLineCount++;
                    result.errorMessage = "Malformed next_id header at line " + std::to_string(lineNumber);
                }
            }
            continue;
        }

        // Skip comment lines
        if (line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string shortCode;
        std::string originalURL;
        int64_t epochSeconds = 0;

        if (iss >> shortCode >> std::quoted(originalURL) >> epochSeconds) {
            if (shortCode.empty() || originalURL.empty()) {
                result.corruptedLineCount++;
                continue;
            }

            // Check for duplicate short codes in the file
            for (const auto& code : seenCodes) {
                if (code == shortCode) {
                    result.corruptedLineCount++;
                    result.errorMessage = "Duplicate short code detected in file: " + shortCode;
                    break;
                }
            }

            seenCodes.push_back(shortCode);
            auto timepoint = URLRecord::fromEpochSeconds(epochSeconds);
            result.records.emplace_back(shortCode, originalURL, timepoint);
        } else {
            result.corruptedLineCount++;
        }
    }

    inFile.close();

    if (result.corruptedLineCount > 0) {
        result.success = false;
        if (result.errorMessage.empty()) {
            result.errorMessage = "Database contains " + std::to_string(result.corruptedLineCount) +
                                  " corrupted or unreadable record(s).";
        }
        return result;
    }

    result.success = true;
    return result;
}

bool FileStorage::save(const std::vector<URLRecord>& records, uint64_t nextId) {
    try {
        if (filePath_.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(filePath_.parent_path(), ec);
            if (ec) {
                std::cerr << "[ERROR] Could not create storage directory: " << ec.message() << std::endl;
                return false;
            }
        }

        // Atomic write via temporary file
        std::filesystem::path tempPath = filePath_;
        tempPath += ".tmp";

        std::ofstream outFile(tempPath, std::ios::trunc);
        if (!outFile.is_open()) {
            std::cerr << "[ERROR] Could not open temp file for writing: " << tempPath << std::endl;
            return false;
        }

        // Write header with metadata
        outFile << "# HASHLINK_STORAGE_V1 next_id=" << nextId << "\n";
        outFile << "# short_code quoted_url epoch_seconds\n";

        for (const auto& rec : records) {
            outFile << rec.getShortCode() << " "
                    << std::quoted(rec.getOriginalURL()) << " "
                    << rec.getEpochSeconds() << "\n";
        }

        outFile.flush();
        if (!outFile.good()) {
            outFile.close();
            std::error_code ec;
            std::filesystem::remove(tempPath, ec);
            return false;
        }
        outFile.close();

        // Atomically replace target file
        std::error_code ec;
        std::filesystem::rename(tempPath, filePath_, ec);
        if (ec) {
            // Fallback: remove target then rename
            std::filesystem::remove(filePath_, ec);
            std::filesystem::rename(tempPath, filePath_, ec);
            if (ec) {
                std::cerr << "[ERROR] Atomic rename failed: " << ec.message() << std::endl;
                return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception during save: " << e.what() << std::endl;
        return false;
    }
}
