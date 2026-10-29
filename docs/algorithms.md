# HashLink CLI — Algorithms & Data Structures Deep-Dive

This document provides a comprehensive mathematical and algorithmic explanation of the Data Structures and Algorithms (ADSA) implemented in **HashLink CLI**.

---

## 1. Hash Table Architecture: Open Addressing vs. Separate Chaining

A **Hash Table** is an associative data structure mapping keys to values using a mathematical hash function.

There are two primary paradigms for resolving hash collisions:
1. **Separate Chaining (Closed Addressing):** Each bucket points to a linked list or auxiliary container of colliding elements.
2. **Open Addressing (Closed Hashing):** All elements reside directly within a single contiguous array. When collisions occur, alternative slots in the array are probed systematically until an available slot is found.

### Why Open Addressing was Chosen for HashLink CLI
* **Cache Locality:** Modern CPU architectures utilize multi-level caches (L1, L2, L3). Open addressing stores slots in a contiguous memory block (`std::vector<HashSlot>`), maximizing cache line hits during linear probing.
* **Zero Pointer Overhead:** Unlike linked lists, open addressing avoids extra heap allocations (`new`/`delete` nodes) and 64-bit pointer overhead per entry.
* **Deterministic Memory Footprint:** The entire table capacity is known and pre-allocated.

---

## 2. Polynomial Rolling Hash Function

A hash function must map an arbitrary-length string $S = s_0 s_1 \dots s_{k-1}$ to an index in the range $[0, \text{capacity} - 1]$ while distributing keys uniformly.

### Mathematical Definition
We use a **Polynomial Rolling Hash**:
$$H(S) = \left( \sum_{i=0}^{k-1} s_i \cdot p^{k-1-i} \right) \pmod m$$
Where:
* $s_i$ is the ASCII integer value of character $i$.
* $p$ is a prime base multiplier. In HashLink CLI, $p = 53$.
* $m$ is the table capacity (a prime number).

### Evaluation via Horner's Rule
Computing $p^i$ directly would cause 64-bit integer overflows and require $O(k)$ exponentiation per term. HashLink CLI evaluates the polynomial iteratively in $O(k)$ using **Horner's Rule**:
$$\text{hash}_0 = 0$$
$$\text{hash}_{i+1} = (\text{hash}_i \cdot p + s_i) \pmod m$$

```cpp
size_t defaultPolynomialHash(const std::string& key, size_t cap) const noexcept {
    const size_t p = 53;
    size_t hashValue = 0;
    for (unsigned char c : key) {
        hashValue = (hashValue * p + c) % cap;
    }
    return hashValue;
}
```

### Why $p = 53$?
Our keys consist of Base62 alphanumeric characters (`0-9`, `a-z`, `A-Z`), spanning 62 distinct symbols. A prime base $p \approx 53$ or $p = 67$ ensures that character positions and combinations do not cancel out modulo powers of 2.

---

## 3. Collision Resolution: Linear Probing

When two distinct keys hash to the same initial index $h(k) = H(k) \pmod m$, a **collision** occurs.

### Probing Sequence
Linear probing checks consecutive slots:
$$\text{index}(k, i) = (h(k) + i) \pmod m \quad \text{for } i = 0, 1, 2, \dots, m-1$$
Where $i$ is the probe step count.

### Primary Clustering
A known theoretical trade-off of linear probing is **primary clustering**: contiguous blocks of occupied slots build up, increasing the average probe length for subsequent keys that hash into or near the cluster.

To mitigate primary clustering, HashLink CLI implements two essential countermeasures:
1. **Prime Capacities:** Table capacities are always prime numbers ($11, 23, 47, \dots$), preventing cyclic sub-loops.
2. **Proactive Resizing ($\alpha \le 0.70$):** Resizing occurs before clusters become long and detrimental.

---

## 4. The Tombstone Deletion Pattern

In open addressing, **deleting an element cannot simply mark the slot as `EMPTY`**.

### The Search Termination Rule
In open addressing, the search algorithm for key $K$ stops when:
1. An `OCCUPIED` slot with matching key is found $\rightarrow$ **Success**.
2. An `EMPTY` slot is encountered $\rightarrow$ **Failure (Key does not exist)**.

### The Broken Chain Problem
Consider a table of size 7:
1. Insert `"A"`: hashes to index 2 $\rightarrow$ placed at `table[2]`.
2. Insert `"B"`: hashes to index 2 $\rightarrow$ collision! Linear probe to `table[3]`.
3. Delete `"A"`:
   * **If naive deletion is used:** `table[2]` is reset to `EMPTY`.
   * **Subsequent search for `"B"`:**
     - Computes $h(\text{"B"}) = 2$.
     - Inspects `table[2]`: sees `EMPTY`.
     - Search terminates immediately and erroneously reports `"B"` does not exist!

```
Initial State:
Index:   0       1       2       3       4
State: [EMPTY] [EMPTY] [OCCUPIED: A] [OCCUPIED: B] [EMPTY]

Naive Delete "A" -> Sets to EMPTY:
Index:   0       1       2       3       4
State: [EMPTY] [EMPTY] [  EMPTY  ] [OCCUPIED: B] [EMPTY]
                            ^
Search "B" stops here! "B" is orphaned and unreachable!
```

### The Solution: Three-State Slots
HashLink CLI introduces three explicit slot states:
* `EMPTY`: Slot has never contained an element. Terminal condition for searches.
* `OCCUPIED`: Slot contains a valid, active `URLRecord`.
* `DELETED` (**Tombstone**): Slot previously held an element that was deleted.

```
Tombstone Delete "A" -> Sets to DELETED:
Index:   0       1       2       3       4
State: [EMPTY] [EMPTY] [ DELETED ] [OCCUPIED: B] [EMPTY]
                            |
Search "B" sees DELETED ----+-> Continues probing to index 3 -> Found!
```

### Tombstone Recycling on Insert
During insertion, the probe sequence notes the index of the first encountered `DELETED` slot. If probing reaches an `EMPTY` slot without finding a duplicate key, the new record is placed directly into the noted tombstone slot, preventing memory waste.

---

## 5. Load Factor ($\alpha$) & Dynamic Rehashing

The **load factor** $\alpha$ quantifies the fullness of the hash table:
$$\alpha = \frac{N_{\text{active}}}{M}$$
Where:
* $N_{\text{active}}$ is the number of active records.
* $M$ is the current table capacity.

### Theoretical Expected Probes
According to Donald Knuth's analysis of linear probing under uniform hashing:
* **Successful search:** $S(\alpha) \approx \frac{1}{2} \left(1 + \frac{1}{1 - \alpha}\right)$
* **Unsuccessful search / Insert:** $U(\alpha) \approx \frac{1}{2} \left(1 + \frac{1}{(1 - \alpha)^2}\right)$

At $\alpha = 0.5$:
* $S(0.5) \approx 1.5$ probes
* $U(0.5) \approx 2.5$ probes

At $\alpha = 0.7$:
* $S(0.7) \approx 2.17$ probes
* $U(0.7) \approx 6.05$ probes

At $\alpha = 0.9$:
* $U(0.9) \approx 50.5$ probes (exponential degradation!)

**Threshold Selection:** HashLink CLI sets the maximum load factor threshold at **$\alpha_{\max} = 0.70$**. Resizing is triggered when $(\text{active} + 1) / M > 0.70$ or when tombstones exhaust open slots.

### Rehashing Mechanics
1. Find next prime $M_{\text{new}} \ge 2 \times M$.
2. Allocate a new slot vector initialized to `EMPTY`.
3. Iterate over the old table: re-insert every `OCCUPIED` record using the new capacity.
4. Purge all `DELETED` tombstones completely, reclaiming table density and resetting probe lengths.

---

## 6. Deterministic Base62 Encoding

Instead of generating non-deterministic random strings (which can collide and exhaust the namespace), short codes are generated deterministically using a monotonically increasing 64-bit integer $ID \in [1, 2^{64}-1]$.

### Base62 Alphabet
$$\Sigma = \{ 0\dots9, a\dots z, A\dots Z \} \quad (|\Sigma| = 62)$$

### Algorithm
```
Input: integer id >= 1
Output: string code

result = ""
while id > 0:
    remainder = id % 62
    result.append(Alphabet[remainder])
    id = id / 62
reverse(result)
return result
```

### Namespace Capacity
* 1 character: $62^1 = 62$ URLs
* 2 characters: $62^2 = 3,844$ URLs
* 3 characters: $62^3 = 238,328$ URLs
* 6 characters: $62^6 \approx 56.8 \times 10^9$ URLs

### Monotonicity & No-Reuse Guarantee
When a shortened URL is deleted, `nextId` is **never decremented**. This guarantees:
1. Old deleted links are never recycled or mapped to a new, unrelated website.
2. Complete audit trail consistency.

---

## 7. Duplicate URL Detection Strategy

When a user submits a long URL that was already shortened, HashLink CLI returns the existing short code rather than duplicating records.

* **Mechanism:** Linear scan through active records in the hash table.
* **Complexity:** $O(N)$ time, where $N$ is the number of active records.
* **Engineering Trade-Off:** In an enterprise production system, a secondary hash table mapping `originalURL -> shortCode` would provide $O(1)$ lookup at the expense of doubling memory overhead and requiring 2-phase synchronized commit/rollback across both tables. For this educational application, an $O(N)$ linear scan keeps memory usage lean and makes the codebase transparent.

---

## 8. Summary of Complexity

| Operation | Best Case | Average Case | Worst Case (Heavy Clustering) | Space Complexity |
| :--- | :---: | :---: | :---: | :---: |
| **Hash Computation** | $O(k)$ | $O(k)$ | $O(k)$ | $O(1)$ |
| **Search by Short Code** | $O(1)$ | $O(1)$ | $O(N)$ | $O(1)$ |
| **Insert New Record** | $O(1)$ | $O(1)$ | $O(N)$ | $O(1)$ |
| **Delete Record (Tombstone)** | $O(1)$ | $O(1)$ | $O(N)$ | $O(1)$ |
| **Dynamic Resizing & Rehash** | $O(N)$ | $O(N)$ | $O(N)$ | $O(M)$ |
| **Duplicate URL Scan** | $O(1)$ | $O(N)$ | $O(N)$ | $O(1)$ |
