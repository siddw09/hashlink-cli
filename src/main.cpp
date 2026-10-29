#include "URLShortener.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <limits>
#include <sstream>

// Helper to safely read a line of input from stdin
std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    // Trim leading and trailing whitespace
    size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = line.find_last_not_of(" \t\r\n");
    return line.substr(first, (last - first + 1));
}

void printHeader(const std::string& title) {
    std::cout << "\n======================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "======================================================\n";
}

void displayMenu() {
    std::cout << "\n+----------------------------------------------------+\n";
    std::cout << "|             HASHLINK CLI - MAIN MENU               |\n";
    std::cout << "+----------------------------------------------------+\n";
    std::cout << "| 1. Shorten a URL                                   |\n";
    std::cout << "| 2. Retrieve Original URL by Short Code             |\n";
    std::cout << "| 3. List All Saved URLs                             |\n";
    std::cout << "| 4. Delete a Shortened URL                          |\n";
    std::cout << "| 5. View Hash Table & System Statistics             |\n";
    std::cout << "| 6. Help & ADSA Algorithm Guide                     |\n";
    std::cout << "| 7. Save & Exit                                     |\n";
    std::cout << "+----------------------------------------------------+\n";
}

void handleShortenURL(URLShortener& shortener) {
    printHeader("SHORTEN A URL");
    std::cout << "Enter a full HTTP/HTTPS URL (e.g., https://example.com/docs):\n";
    std::string url = readLine("> ");

    if (url.empty()) {
        std::cout << "[ERROR] Input cannot be blank.\n";
        return;
    }

    ShortenResult res = shortener.shortenURL(url);
    if (!res.success) {
        std::cout << "\n[FAILED] " << res.message << "\n";
    } else {
        std::cout << "\n------------------------------------------------------\n";
        if (res.isDuplicate) {
            std::cout << "[NOTICE] URL already exists in database!\n";
        } else {
            std::cout << "[SUCCESS] New URL shortened successfully!\n";
        }
        std::cout << "Original URL : " << url << "\n";
        std::cout << "Short Code   : " << res.shortCode << "\n";
        std::cout << "------------------------------------------------------\n";
    }
}

void handleRetrieveURL(const URLShortener& shortener) {
    printHeader("RETRIEVE ORIGINAL URL");
    std::string code = readLine("Enter short code (e.g., 1, a, 10): ");

    if (code.empty()) {
        std::cout << "[ERROR] Short code cannot be empty.\n";
        return;
    }

    auto maybeUrl = shortener.getOriginalURL(code);
    if (maybeUrl.has_value()) {
        std::cout << "\n------------------------------------------------------\n";
        std::cout << "Short Code   : " << code << "\n";
        std::cout << "Original URL : " << maybeUrl.value() << "\n";
        std::cout << "------------------------------------------------------\n";
    } else {
        std::cout << "\n[NOT FOUND] No record matches short code: '" << code << "'\n";
    }
}

void handleListURLs(const URLShortener& shortener) {
    printHeader("LIST OF ALL SAVED URLS");
    auto records = shortener.listURLs();

    if (records.empty()) {
        std::cout << "[INFO] No URLs currently stored in the database.\n";
        return;
    }

    std::cout << std::left
              << std::setw(12) << "Short Code"
              << std::setw(26) << "Created At"
              << "Original URL" << "\n";
    std::cout << std::string(72, '-') << "\n";

    for (const auto& rec : records) {
        std::cout << std::left
                  << std::setw(12) << rec.getShortCode()
                  << std::setw(26) << rec.getFormattedTimestamp()
                  << rec.getOriginalURL() << "\n";
    }

    std::cout << std::string(72, '-') << "\n";
    std::cout << "Total Active Records: " << records.size() << "\n";
}

void handleDeleteURL(URLShortener& shortener) {
    printHeader("DELETE A SHORTENED URL");
    std::string code = readLine("Enter short code to delete: ");

    if (code.empty()) {
        std::cout << "[ERROR] Short code cannot be empty.\n";
        return;
    }

    bool deleted = shortener.deleteURL(code);
    if (deleted) {
        std::cout << "\n[SUCCESS] Record with short code '" << code
                  << "' has been deleted.\n";
        std::cout << "[NOTE] The slot is marked as a TOMBSTONE to preserve linear probe paths.\n";
    } else {
        std::cout << "\n[FAILED] Record not found for short code: '" << code << "'\n";
    }
}

void handleViewStats(const URLShortener& shortener) {
    printHeader("DATA STRUCTURES & ALGORITHMS (ADSA) METRICS");
    ShortenerStats stats = shortener.getStatistics();

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Hash Table Capacity          : " << stats.capacity << " slots\n";
    std::cout << "Active Records               : " << stats.activeRecords << "\n";
    std::cout << "Tombstone (Deleted) Slots    : " << stats.tombstoneCount << "\n";
    std::cout << "Current Load Factor (alpha)  : " << stats.loadFactor
              << " (Threshold: " << stats.maxLoadFactor << ")\n";
    std::cout << "Dynamic Resize Operations    : " << stats.resizeCount << "\n";
    std::cout << "Cumulative Collision Events  : " << stats.collisionEvents << "\n";
    std::cout << "Cumulative Probe Steps       : " << stats.totalProbeSteps << "\n";
    std::cout << "Next Monotonic ID (Base62)   : " << stats.nextId
              << " (Encodes as: " << URLShortener::encodeBase62(stats.nextId) << ")\n";
    std::cout << "Database File Path           : " << stats.dbPath << "\n";

    std::cout << "\n--- Collision Metric Explanation ---\n";
    std::cout << "1. Collision Events: Count of insertions where initial bucket was already occupied.\n";
    std::cout << "2. Probe Steps: Total slot inspections performed across insert, find, and delete.\n";
    std::cout << "3. Load Factor: active_records / capacity. Resizing triggers when alpha > "
              << stats.maxLoadFactor << ".\n";
}

void handleHelp() {
    printHeader("ALGORITHMS & ARCHITECTURE GUIDE");
    std::cout << R"(
HashLink CLI is designed as an educational, production-grade showcase of
fundamental Data Structures and Algorithms (ADSA) in C++17.

1. CUSTOM HASH TABLE ARCHITECTURE:
   - Built from scratch using std::vector of slots (no std::unordered_map).
   - Slot states: EMPTY (unvisited), OCCUPIED (active), DELETED (tombstone).

2. POLYNOMIAL ROLLING HASH FUNCTION:
   - Computes hash = sum(c[i] * p^(k-1-i)) mod capacity, using prime p = 53.
   - Evaluated via Horner's rule in O(k) time, preventing integer overflow.

3. LINEAR PROBING COLLISION RESOLUTION:
   - Probe sequence: index = (hash(shortCode) + probeNumber) % capacity.
   - Provides excellent CPU cache locality compared to separate chaining.

4. TOMBSTONE DELETION PATTERN:
   - When deleting, the slot is marked as DELETED instead of EMPTY.
   - Why? If marked EMPTY, searches for elements inserted past this slot
     would prematurely abort and return "not found".
   - Insertions reuse tombstone slots, maintaining space efficiency.

5. BASE62 SHORT CODE GENERATION:
   - Monotonically increasing 64-bit integer encoded into Base62 [0-9a-zA-Z].
   - IDs are never reused upon deletion, preventing URL collision & stale links.

6. TIME & SPACE COMPLEXITY:
   - Insert/Lookup/Delete (Average): O(1) time
   - Worst-case (Heavy clustering) : O(N) time
   - Duplicate URL Scan            : O(N) linear scan
   - Memory Overhead               : O(Capacity) space
)";
}

int main(int argc, char* argv[]) {
    std::string dbPath = "data/hashlink.db";
    if (argc > 1) {
        dbPath = argv[1];
    }

    std::cout << "======================================================\n";
    std::cout << "      HashLink CLI - C++17 URL Shortener             \n";
    std::cout << "   Advanced Data Structures & Algorithms Project      \n";
    std::cout << "======================================================\n";

    URLShortener shortener(dbPath);
    std::string loadStatus;
    if (!shortener.loadFromDisk(loadStatus)) {
        std::cerr << "\n" << loadStatus << "\n\n";
        std::cout << "Press Enter to proceed in read-only / emergency mode...";
        std::string dummy;
        std::getline(std::cin, dummy);
    } else {
        std::cout << loadStatus << "\n";
    }

    bool running = true;
    while (running) {
        displayMenu();
        std::string choiceStr = readLine("Select an option (1-7): ");

        if (choiceStr.empty()) {
            continue;
        }

        int choice = 0;
        try {
            choice = std::stoi(choiceStr);
        } catch (...) {
            std::cout << "\n[ERROR] Invalid input. Please enter a number between 1 and 7.\n";
            continue;
        }

        switch (choice) {
            case 1:
                handleShortenURL(shortener);
                break;
            case 2:
                handleRetrieveURL(shortener);
                break;
            case 3:
                handleListURLs(shortener);
                break;
            case 4:
                handleDeleteURL(shortener);
                break;
            case 5:
                handleViewStats(shortener);
                break;
            case 6:
                handleHelp();
                break;
            case 7: {
                printHeader("SAVING & EXITING");
                if (shortener.isStorageCorrupted()) {
                    std::cout << "[WARNING] Storage was flagged corrupted; skipping auto-save.\n";
                } else {
                    if (shortener.saveToDisk()) {
                        std::cout << "[SUCCESS] All records successfully persisted to "
                                  << dbPath << ".\n";
                    } else {
                        std::cerr << "[ERROR] Failed to save records during exit.\n";
                    }
                }
                std::cout << "Goodbye!\n";
                running = false;
                break;
            }
            default:
                std::cout << "\n[ERROR] Invalid option (" << choice
                          << "). Please choose between 1 and 7.\n";
                break;
        }
    }

    return 0;
}
