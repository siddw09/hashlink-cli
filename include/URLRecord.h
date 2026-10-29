#ifndef URL_RECORD_H
#define URL_RECORD_H

#include <string>
#include <chrono>

/**
 * @brief Represents a single shortened URL record.
 * 
 * Encapsulates the short code, the original long URL, and the timestamp
 * when the record was created.
 */
class URLRecord {
private:
    std::string shortCode_;
    std::string originalURL_;
    std::chrono::system_clock::time_point createdAt_;

public:
    /**
     * @brief Default constructor for an empty record.
     */
    URLRecord();

    /**
     * @brief Parameterized constructor using current system time.
     * @param shortCode Deterministic Base62 shortened code.
     * @param originalURL Validated original long URL.
     */
    URLRecord(std::string shortCode, std::string originalURL);

    /**
     * @brief Parameterized constructor with explicit timestamp.
     * @param shortCode Deterministic Base62 shortened code.
     * @param originalURL Validated original long URL.
     * @param createdAt Timepoint of creation.
     */
    URLRecord(std::string shortCode, std::string originalURL,
              std::chrono::system_clock::time_point createdAt);

    // Accessors
    const std::string& getShortCode() const noexcept;
    const std::string& getOriginalURL() const noexcept;
    std::chrono::system_clock::time_point getCreatedAt() const noexcept;

    /**
     * @brief Returns a human-readable formatted timestamp string (UTC).
     * Format: YYYY-MM-DD HH:MM:SS
     */
    std::string getFormattedTimestamp() const;

    /**
     * @brief Returns seconds since Unix epoch for persistent storage.
     */
    int64_t getEpochSeconds() const noexcept;

    /**
     * @brief Reconstructs a time_point from Unix epoch seconds.
     */
    static std::chrono::system_clock::time_point fromEpochSeconds(int64_t seconds);

    /**
     * @brief Checks if this record has a non-empty short code.
     */
    bool isValid() const noexcept;

    // Equality operator based on shortCode and originalURL
    bool operator==(const URLRecord& other) const noexcept;
    bool operator!=(const URLRecord& other) const noexcept;
};

#endif // URL_RECORD_H
