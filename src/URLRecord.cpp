#include "URLRecord.h"
#include <iomanip>
#include <sstream>
#include <ctime>

URLRecord::URLRecord()
    : shortCode_(""), originalURL_(""), createdAt_(std::chrono::system_clock::time_point{}) {}

URLRecord::URLRecord(std::string shortCode, std::string originalURL)
    : shortCode_(std::move(shortCode)),
      originalURL_(std::move(originalURL)),
      createdAt_(std::chrono::system_clock::now()) {}

URLRecord::URLRecord(std::string shortCode, std::string originalURL,
                     std::chrono::system_clock::time_point createdAt)
    : shortCode_(std::move(shortCode)),
      originalURL_(std::move(originalURL)),
      createdAt_(createdAt) {}

const std::string& URLRecord::getShortCode() const noexcept {
    return shortCode_;
}

const std::string& URLRecord::getOriginalURL() const noexcept {
    return originalURL_;
}

std::chrono::system_clock::time_point URLRecord::getCreatedAt() const noexcept {
    return createdAt_;
}

std::string URLRecord::getFormattedTimestamp() const {
    std::time_t timeT = std::chrono::system_clock::to_time_t(createdAt_);
    std::tm timeBuf{};
#if defined(_WIN32)
    gmtime_s(&timeBuf, &timeT);
#else
    gmtime_r(&timeT, &timeBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&timeBuf, "%Y-%m-%d %H:%M:%S UTC");
    return oss.str();
}

int64_t URLRecord::getEpochSeconds() const noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
        createdAt_.time_since_epoch()).count();
}

std::chrono::system_clock::time_point URLRecord::fromEpochSeconds(int64_t seconds) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}

bool URLRecord::isValid() const noexcept {
    return !shortCode_.empty() && !originalURL_.empty();
}

bool URLRecord::operator==(const URLRecord& other) const noexcept {
    return shortCode_ == other.shortCode_ && originalURL_ == other.originalURL_;
}

bool URLRecord::operator!=(const URLRecord& other) const noexcept {
    return !(*this == other);
}
