# HashLink CLI — 

This guide prepares you to answer theoretical and implementation questions during your Data Structures & Algorithms (ADSA) practical examination or project presentation.

---

### Q1: What is the primary objective of HashLink CLI?
**Answer:**
HashLink CLI is a terminal-based URL shortening application built in modern C++17. Its primary academic objective is to demonstrate the internal mechanics of a **custom Open-Addressing Hash Table** with **Linear Probing** and **Tombstone-based deletion**, without relying on standard library hash containers (`std::unordered_map`). It stores URL records locally and provides persistence, statistics, and deterministic Base62 encoding.

---

### Q2: Why did you implement a custom hash table instead of using `std::unordered_map`?
**Answer:**
`std::unordered_map` in the C++ Standard Library is implemented using **Separate Chaining** (arrays of linked list nodes). Implementing our own table allows us to:
1. Demonstrate **Open Addressing with Linear Probing**, maximizing CPU cache locality.
2. Directly implement and instrument **Tombstone deletion**, **probe length tracking**, and **dynamic resizing**.
3. Showcase internal ADSA competencies required for academic evaluation.

---

### Q3: What is a hash collision? How does HashLink resolve collisions?
**Answer:**
A hash collision occurs when two different keys $k_1 \ne k_2$ evaluate to the same initial array index under the hash function ($h(k_1) = h(k_2)$).
HashLink CLI resolves collisions using **Open Addressing with Linear Probing**. If bucket `initialIndex` is already occupied, the algorithm systematically checks consecutive buckets:
$$\text{index} = (\text{initialIndex} + i) \pmod{\text{capacity}} \quad \text{for } i = 0, 1, 2, \dots$$

---

### Q4: What is a "Tombstone" and why is it mandatory in open addressing?
**Answer:**
In open addressing, an empty slot (`EMPTY`) signals the termination of a search. If we simply reset a deleted slot back to `EMPTY`, any record that was inserted *after* that slot due to a collision probe would become unreachable—the search would stop prematurely at the empty slot and report "not found".

To prevent this "broken probe chain" bug, we mark deleted slots with a special state called a **Tombstone** (`DELETED`):
* **Search operations** continue probing past tombstones.
* **Insert operations** can reuse tombstone slots for new records if the key is not already present.
* **Dynamic rehashing** purges all tombstones, restoring table efficiency.

---

### Q5: How does your hash function work? Why did you pick Polynomial Rolling Hash?
**Answer:**
We implemented a **Polynomial Rolling Hash** using the formula:
$$H(S) = \left( \sum_{i=0}^{k-1} s_i \cdot p^{k-1-i} \right) \pmod m$$
Where $p = 53$ (a prime multiplier) and $m$ is the table capacity (a prime number).
* We evaluate this in $O(k)$ time using **Horner’s Rule**:
  `hash = (hash * 53 + c) % capacity`.
* Performing modulo arithmetic at each step prevents integer overflow while maintaining excellent avalanche properties across alphanumeric strings.

---

### Q6: What is the Load Factor ($\alpha$), and what is your resizing threshold?
**Answer:**
The load factor is the ratio of active elements to total table capacity:
$$\alpha = \frac{N_{\text{active}}}{M_{\text{capacity}}}$$
In HashLink CLI, the maximum load factor threshold is **$\alpha = 0.70$ (70%)**. When the load factor exceeds 0.70, or when tombstones fill the remaining empty slots, the table automatically resizes and rehashes.

---

### Q7: Why not wait until the load factor reaches 1.0 (100%) before resizing?
**Answer:**
In linear probing, search and insertion time degrades exponentially as $\alpha$ approaches 1.0. Donald Knuth proved that the expected number of probes for an unsuccessful search is:
$$U(\alpha) \approx \frac{1}{2} \left(1 + \frac{1}{(1 - \alpha)^2}\right)$$
At $\alpha = 0.7$, the expected probe count is $\approx 6$. At $\alpha = 0.9$, it jumps to $\approx 50$. At $\alpha = 1.0$, insertion would enter an infinite loop. Resizing at $0.70$ keeps average operations strictly $O(1)$.

---

### Q8: Why are table capacities always chosen as prime numbers?
**Answer:**
Using a prime table capacity ensures that $\gcd(p, M) = 1$ (the hash multiplier and table size are coprime). If the table size shared common factors with the hash multiplier or key values, linear probing would only visit a fraction of the table slots (a sub-cycle), leading to severe clustering. Prime capacities ensure uniform distribution and full probe coverage.

---

### Q9: What is Primary Clustering in linear probing?
**Answer:**
Primary clustering is the tendency of occupied slots to form long contiguous blocks. Once a cluster forms, any key that hashes anywhere into the cluster lengthens it further, compounding the collision rate. We mitigate primary clustering through prime table sizing and proactive rehashing at $\alpha \le 0.70$.

---

### Q10: How does short-code generation work? Why Base62?
**Answer:**
Short codes are generated deterministically by converting a monotonically increasing 64-bit integer (`nextId`) into a Base62 string using the character set `[0-9a-zA-Z]` (10 digits + 26 lowercase + 26 uppercase = 62 characters).
* **Why Base62?** Unlike Base64, Base62 does not use `+`, `/`, or `=`, which have special syntactic meanings in URL query strings and path segments. It produces URL-safe, compact codes.
* 6 Base62 characters can represent over 56.8 billion unique URLs ($62^6$).

---

### Q11: What happens to the `nextId` when a record is deleted? Is it reused?
**Answer:**
`nextId` is **never decremented or reused** upon deletion. Reusing IDs could cause stale browser bookmarks or external links to mistakenly resolve to a new, unrelated URL. Monotonicity ensures persistent uniqueness.

---

### Q12: How do you handle duplicate original URLs?
**Answer:**
When a user attempts to shorten a URL, HashLink CLI first performs a linear scan through active records. If the original URL is already present, it returns the existing short code rather than creating a duplicate record.
* **Complexity:** $O(N)$ time.
* **Exam note:** In a multi-billion record production database, a secondary hash index mapping `URL -> ShortCode` would provide $O(1)$ duplicate checking at the expense of doubling memory overhead. For our project, $O(N)$ linear scan keeps memory lean and avoids distributed synchronization complexity.

---

### Q13: How does persistent storage work? How is data protected against corruption?
**Answer:**
Storage is managed by `FileStorage`:
1. **Quoted Strings:** Records are serialized using `std::quoted`, ensuring that URLs containing spaces, commas, or quotes are safely parsed upon reloading.
2. **Atomic Writes:** Data is written to a temporary `.tmp` file and then renamed using `std::filesystem::rename`. If power fails or the process crashes mid-write, the existing database file remains intact.
3. **Corruption Protection:** If corrupted records or duplicate keys are detected on startup, HashLink CLI enters emergency protection mode and disables auto-save, preventing valid data from being overwritten by an empty database.

---

### Q14: What are the time and space complexities of your operations?
**Answer:**
* **Search / Find:** Average $O(1)$, Worst-case $O(N)$ (if extreme clustering occurs).
* **Insert:** Average $O(1)$, Worst-case $O(N)$ (amortized $O(1)$ when factoring in dynamic resizing).
* **Delete:** Average $O(1)$, Worst-case $O(N)$.
* **Space Complexity:** $O(M)$ where $M$ is the table capacity ($M = \Theta(N)$ since $\alpha \le 0.70$).

---

### Q15: How did you test your hash table deterministically?
**Answer:**
In `HashTable`, we designed the constructor to accept an optional custom hash function (`std::function<size_t(const std::string&, size_t)>`). In our automated test suite (`test_hash_table.cpp`), we inject a forced constant hash `[](const std::string&, size_t) { return 2; }`. This allows us to deterministically prove:
1. Linear probing places colliding keys at indices 2, 3, and 4.
2. Deleting index 3 leaves a tombstone.
3. Lookup for index 4 successfully navigates past the tombstone at index 3.
4. Inserting a new key reuses the tombstone at index 3.

---

### Q16: How do you prevent infinite probing loops?
**Answer:**
In all three probing loops (`insert`, `find`, `remove`), the loop index is strictly bounded: `for (size_t probe = 0; probe < capacity_; ++probe)`. Even if every slot in the table were occupied or marked as a tombstone, the loop halts after checking `capacity_` slots, eliminating any possibility of an infinite loop.

---

### Q17: What C++17 standard features did you leverage?
**Answer:**
* `std::filesystem::path`, `exists`, `create_directories`, and `rename` for portable file persistence.
* `std::optional<std::string>` for expressive, null-safe lookup return values.
* `std::quoted` for delimiter-safe string serialization.
* `std::chrono::system_clock` for precise timestamps.
* `enum class SlotState` for strongly-typed, scoped state enumeration.
* RAII (Resource Acquisition Is Initialization) for automated memory management and file streams.
