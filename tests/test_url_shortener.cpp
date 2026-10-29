#include "URLShortener.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

void testBase62Encoding() {
    std::cout << "[RUN] Testing Base62 encoding and decoding..." << std::endl;
    assert(URLShortener::encodeBase62(0) == "0");
    assert(URLShortener::encodeBase62(1) == "1");
    assert(URLShortener::encodeBase62(61) == "Z");
    assert(URLShortener::encodeBase62(62) == "10");
    assert(URLShortener::encodeBase62(2205) == "zz");
    assert(URLShortener::encodeBase62(3843) == "ZZ");

    assert(URLShortener::decodeBase62("0") == 0);
    assert(URLShortener::decodeBase62("1") == 1);
    assert(URLShortener::decodeBase62("Z") == 61);
    assert(URLShortener::decodeBase62("10") == 62);
    assert(URLShortener::decodeBase62("zz") == 2205);
    assert(URLShortener::decodeBase62("ZZ") == 3843);
    std::cout << "[PASS] Base62 encoding and decoding passed." << std::endl;
}

void testURLValidation() {
    std::cout << "[RUN] Testing URL validation..." << std::endl;
    std::string err;

    // Valid URLs
    assert(URLShortener::validateURL("https://google.com", err) == true);
    assert(URLShortener::validateURL("http://localhost:8080/path?query=1#hash", err) == true);
    assert(URLShortener::validateURL("https://sub.domain.org/api/v1/resource", err) == true);

    // Invalid URLs
    assert(URLShortener::validateURL("", err) == false);
    assert(URLShortener::validateURL("ftp://files.example.com", err) == false);
    assert(URLShortener::validateURL("just-a-string", err) == false);
    assert(URLShortener::validateURL("https:// with spaces.com", err) == false);
    assert(URLShortener::validateURL("https://", err) == false);
    assert(URLShortener::validateURL("http:///no-host", err) == false);

    std::cout << "[PASS] URL validation passed." << std::endl;
}

void testShortenAndDuplicateDetection() {
    std::cout << "[RUN] Testing shortening and duplicate detection..." << std::endl;
    std::string testDb = "data/test_duplicate.db";
    std::filesystem::remove(testDb);

    URLShortener shortener(testDb);
    auto res1 = shortener.shortenURL("https://github.com");
    assert(res1.success == true);
    assert(res1.isDuplicate == false);
    std::string code1 = res1.shortCode;

    // Shorten exact same URL again -> must return existing code
    auto res2 = shortener.shortenURL("https://github.com");
    assert(res2.success == true);
    assert(res2.isDuplicate == true);
    assert(res2.shortCode == code1);
    assert(shortener.listURLs().size() == 1);

    // Shorten different URL
    auto res3 = shortener.shortenURL("https://en.wikipedia.org");
    assert(res3.success == true);
    assert(res3.isDuplicate == false);
    assert(res3.shortCode != code1);
    assert(shortener.listURLs().size() == 2);

    std::filesystem::remove(testDb);
    std::cout << "[PASS] Shortening and duplicate detection passed." << std::endl;
}

void testDeleteAndMonotonicId() {
    std::cout << "[RUN] Testing deletion and monotonic ID retention..." << std::endl;
    std::string testDb = "data/test_delete.db";
    std::filesystem::remove(testDb);

    URLShortener shortener(testDb);
    auto res1 = shortener.shortenURL("https://site1.com");
    auto res2 = shortener.shortenURL("https://site2.com");
    uint64_t currentNextId = shortener.getNextId();

    // Delete the most recently added record (highest ID)
    assert(shortener.deleteURL(res2.shortCode) == true);
    assert(shortener.listURLs().size() == 1);

    // CRITICAL: Next ID must NOT be reused or decremented
    assert(shortener.getNextId() == currentNextId);

    // Next shortened URL must get a fresh, higher ID
    auto res3 = shortener.shortenURL("https://site3.com");
    assert(res3.shortCode != res2.shortCode);

    std::filesystem::remove(testDb);
    std::cout << "[PASS] Deletion and monotonic ID retention passed." << std::endl;
}

void testPersistenceAcrossInstances() {
    std::cout << "[RUN] Testing file persistence across application instances..." << std::endl;
    std::string testDb = "data/test_persistence.db";
    std::filesystem::remove(testDb);

    std::string codeA, codeB;
    uint64_t nextIdBeforeExit;

    {
        // First instance
        URLShortener shortener(testDb);
        auto r1 = shortener.shortenURL("https://alpha.org/path?name=john&age=30");
        auto r2 = shortener.shortenURL("https://beta.com/complex/\"quoted\"/url");
        assert(r1.success);
        assert(r2.success);
        codeA = r1.shortCode;
        codeB = r2.shortCode;
        nextIdBeforeExit = shortener.getNextId();
    }

    {
        // Second instance loading from same database file
        URLShortener newShortener(testDb);
        std::string status;
        bool loaded = newShortener.loadFromDisk(status);
        assert(loaded == true);
        assert(newShortener.listURLs().size() == 2);

        // Verify records accurately restored (including complex characters)
        auto origA = newShortener.getOriginalURL(codeA);
        assert(origA.has_value());
        assert(origA.value() == "https://alpha.org/path?name=john&age=30");

        auto origB = newShortener.getOriginalURL(codeB);
        assert(origB.has_value());
        assert(origB.value() == "https://beta.com/complex/\"quoted\"/url");

        // Verify monotonic ID resumed correctly
        assert(newShortener.getNextId() >= nextIdBeforeExit);
    }

    std::filesystem::remove(testDb);
    std::cout << "[PASS] File persistence across application instances passed." << std::endl;
}

void testCorruptedDatabaseHandling() {
    std::cout << "[RUN] Testing corrupted database handling..." << std::endl;
    std::string testDb = "data/test_corrupt.db";
    std::filesystem::remove(testDb);

    // Write a corrupted file
    {
        std::ofstream corruptFile(testDb);
        corruptFile << "# HASHLINK_STORAGE_V1 next_id=10\n";
        corruptFile << "bad_record_missing_fields\n"; // malformed line
        corruptFile.close();
    }

    URLShortener shortener(testDb);
    std::string statusMsg;
    bool loadSuccess = shortener.loadFromDisk(statusMsg);

    // Must report error and activate corruption protection
    assert(loadSuccess == false);
    assert(shortener.isStorageCorrupted() == true);

    // Ensure saveToDisk() is blocked so corrupt file is NOT overwritten
    bool saveResult = shortener.saveToDisk();
    assert(saveResult == false);

    std::filesystem::remove(testDb);
    std::cout << "[PASS] Corrupted database handling passed." << std::endl;
}

void testEmptyDatabaseAndMissingFileBehavior() {
    std::cout << "[RUN] Testing empty database and missing file behavior..." << std::endl;
    std::string testDb = "data/test_missing_clean.db";
    std::filesystem::remove(testDb);

    URLShortener shortener(testDb);
    std::string statusMsg;
    bool loaded = shortener.loadFromDisk(statusMsg);
    assert(loaded == true);
    assert(shortener.listURLs().empty());
    assert(shortener.getOriginalURL("any").has_value() == false);
    assert(shortener.deleteURL("any") == false);
    assert(shortener.getStatistics().activeRecords == 0);

    std::cout << "[PASS] Empty database and missing file behavior passed." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    RUNNING URL SHORTENER UNIT TESTS    " << std::endl;
    std::cout << "========================================" << std::endl;

    testEmptyDatabaseAndMissingFileBehavior();
    testBase62Encoding();
    testURLValidation();
    testShortenAndDuplicateDetection();
    testDeleteAndMonotonicId();
    testPersistenceAcrossInstances();
    testCorruptedDatabaseHandling();

    std::cout << "========================================" << std::endl;
    std::cout << "  ALL URL SHORTENER TESTS PASSED (7/7)  " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
