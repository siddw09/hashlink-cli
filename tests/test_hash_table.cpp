#include "HashTable.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

void testBasicInsertAndFind() {
    std::cout << "[RUN] Testing basic insertion and retrieval..." << std::endl;
    HashTable ht(11);

    URLRecord rec1("abc", "https://example.com");
    URLRecord rec2("xyz", "https://google.com");

    assert(ht.insert(rec1) == true);
    assert(ht.insert(rec2) == true);
    assert(ht.size() == 2);

    const URLRecord* found1 = ht.find("abc");
    assert(found1 != nullptr);
    assert(found1->getOriginalURL() == "https://example.com");

    const URLRecord* found2 = ht.find("xyz");
    assert(found2 != nullptr);
    assert(found2->getOriginalURL() == "https://google.com");

    // Missing key
    assert(ht.find("nonexistent") == nullptr);
    assert(ht.contains("nonexistent") == false);
    assert(ht.contains("abc") == true);

    std::cout << "[PASS] Basic insertion and retrieval passed." << std::endl;
}

void testDuplicateKeyRejection() {
    std::cout << "[RUN] Testing duplicate key rejection..." << std::endl;
    HashTable ht(11);

    URLRecord rec1("code1", "https://site1.com");
    URLRecord rec2("code1", "https://site2.com"); // Same code, different URL

    assert(ht.insert(rec1) == true);
    assert(ht.insert(rec2) == false); // Should reject duplicate
    assert(ht.size() == 1);

    const URLRecord* found = ht.find("code1");
    assert(found != nullptr);
    assert(found->getOriginalURL() == "https://site1.com"); // Not overwritten

    std::cout << "[PASS] Duplicate key rejection passed." << std::endl;
}

void testForcedCollisionsAndLinearProbing() {
    std::cout << "[RUN] Testing forced collisions and linear probing..." << std::endl;
    
    // Inject custom hash function that forces all keys to hash to bucket 2
    auto forcedHash = [](const std::string&, size_t) -> size_t {
        return 2;
    };

    HashTable ht(11, 0.9, forcedHash);

    URLRecord r1("keyA", "https://a.com");
    URLRecord r2("keyB", "https://b.com");
    URLRecord r3("keyC", "https://c.com");

    assert(ht.insert(r1) == true);
    assert(ht.insert(r2) == true);
    assert(ht.insert(r3) == true);

    // Verify slots are placed consecutively via linear probing:
    // Slot 2: keyA
    // Slot 3: keyB (probed 1)
    // Slot 4: keyC (probed 2)
    assert(ht.getSlotState(2) == SlotState::OCCUPIED);
    assert(ht.getSlotRecord(2)->getShortCode() == "keyA");

    assert(ht.getSlotState(3) == SlotState::OCCUPIED);
    assert(ht.getSlotRecord(3)->getShortCode() == "keyB");

    assert(ht.getSlotState(4) == SlotState::OCCUPIED);
    assert(ht.getSlotRecord(4)->getShortCode() == "keyC");

    // Verify lookup works across probe chain
    assert(ht.find("keyA") != nullptr && ht.find("keyA")->getOriginalURL() == "https://a.com");
    assert(ht.find("keyB") != nullptr && ht.find("keyB")->getOriginalURL() == "https://b.com");
    assert(ht.find("keyC") != nullptr && ht.find("keyC")->getOriginalURL() == "https://c.com");

    // Collision stats should reflect 2 collisions (keyB and keyC)
    assert(ht.getCollisionCount() == 2);

    std::cout << "[PASS] Forced collisions and linear probing passed." << std::endl;
}

void testTombstoneDeletionAndProbeContinuation() {
    std::cout << "[RUN] Testing tombstone deletion and probe continuation..." << std::endl;

    // Force collision at bucket 1: keys A, B, C hash to slot 1
    auto forcedHash = [](const std::string&, size_t) -> size_t {
        return 1;
    };

    HashTable ht(11, 0.9, forcedHash);
    ht.insert(URLRecord("A", "https://a.com"));
    ht.insert(URLRecord("B", "https://b.com"));
    ht.insert(URLRecord("C", "https://c.com"));

    // Slot 1 = A, Slot 2 = B, Slot 3 = C
    assert(ht.getSlotState(1) == SlotState::OCCUPIED);
    assert(ht.getSlotState(2) == SlotState::OCCUPIED);
    assert(ht.getSlotState(3) == SlotState::OCCUPIED);

    // Delete "B" (middle of probe chain)
    assert(ht.remove("B") == true);
    assert(ht.size() == 2);
    assert(ht.tombstoneCount() == 1);

    // Slot 2 must now be DELETED (tombstone)
    assert(ht.getSlotState(2) == SlotState::DELETED);

    // CRITICAL: Searching for "C" must skip over the tombstone at slot 2 and find "C" at slot 3
    const URLRecord* foundC = ht.find("C");
    assert(foundC != nullptr);
    assert(foundC->getOriginalURL() == "https://c.com");

    // Searching for "B" must return nullptr
    assert(ht.find("B") == nullptr);

    // Now insert a new key "D", which also hashes to 1.
    // Probing: slot 1 (occupied A), slot 2 (tombstone).
    // It should reuse the tombstone slot 2!
    assert(ht.insert(URLRecord("D", "https://d.com")) == true);
    assert(ht.getSlotState(2) == SlotState::OCCUPIED);
    assert(ht.getSlotRecord(2)->getShortCode() == "D");
    assert(ht.tombstoneCount() == 0);

    // Both "D" and "C" must be retrievable
    assert(ht.find("D") != nullptr);
    assert(ht.find("C") != nullptr);

    std::cout << "[PASS] Tombstone deletion and probe continuation passed." << std::endl;
}

void testDynamicResizingAndRehashing() {
    std::cout << "[RUN] Testing dynamic resizing and rehashing..." << std::endl;

    // Small table of initial capacity 5, max load factor 0.7 (max active before resize = 5 * 0.7 = 3)
    HashTable ht(5, 0.7);
    size_t initialCap = ht.capacity();
    assert(initialCap >= 5);

    for (int i = 0; i < 20; ++i) {
        std::string code = "code" + std::to_string(i);
        std::string url = "https://example.com/" + std::to_string(i);
        assert(ht.insert(URLRecord(code, url)) == true);
    }

    assert(ht.size() == 20);
    assert(ht.capacity() > initialCap);
    assert(ht.getResizeCount() > 0);
    assert(ht.loadFactor() <= ht.maxLoadFactor());

    // Verify all 20 records remain intact after multiple resizes and rehashes
    for (int i = 0; i < 20; ++i) {
        std::string code = "code" + std::to_string(i);
        const URLRecord* rec = ht.find(code);
        assert(rec != nullptr);
        assert(rec->getOriginalURL() == "https://example.com/" + std::to_string(i));
    }

    std::cout << "[PASS] Dynamic resizing and rehashing passed." << std::endl;
}

void testEmptyTableBehavior() {
    std::cout << "[RUN] Testing empty table behavior..." << std::endl;
    HashTable ht(11);
    assert(ht.size() == 0);
    assert(ht.loadFactor() == 0.0);
    assert(ht.find("anything") == nullptr);
    assert(ht.contains("anything") == false);
    assert(ht.remove("anything") == false);
    assert(ht.getAllRecords().empty());
    std::cout << "[PASS] Empty table behavior passed." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "      RUNNING HASH TABLE UNIT TESTS     " << std::endl;
    std::cout << "========================================" << std::endl;

    testEmptyTableBehavior();
    testBasicInsertAndFind();
    testDuplicateKeyRejection();
    testForcedCollisionsAndLinearProbing();
    testTombstoneDeletionAndProbeContinuation();
    testDynamicResizingAndRehashing();

    std::cout << "========================================" << std::endl;
    std::cout << "    ALL HASH TABLE TESTS PASSED (6/6)   " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
