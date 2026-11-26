# Custom Data Structures User Guide

These four class templates live in the `customTrie` namespace. Add the project's `include/` directory to the header search path, then include `team/trie.h`, `team/hashtable.h`, `team/list.h`, or `team/vector.h` as needed. Each header includes its corresponding template implementation file, so these `.cpp` files do not need to be compiled separately.

## trie: hierarchical topics and subscribers

`trie<T>` splits topic paths on `/` and stores pointers to subscriber objects. A node ID is an index into the trie's internal array: the root has ID 0, and other IDs are assigned in creation order. IDs belong to the trie that created them and must not be used with another trie.

| Function | Behavior |
| --- | --- |
| `trie()` | Creates an empty trie containing a root node. |
| `findTopic(path, node)` | Finds an exact path. On success, writes its node ID to `node` and returns `true`; otherwise returns `false`. Invalid paths return `false`. |
| `ensureTopic(path)` | Creates missing nodes and returns the final node ID. The caller must provide a non-null, non-empty path with no leading, trailing, or consecutive `/` characters; this function does not validate input. |
| `addSubscription(node, subscriber)` | Adds the subscriber object's address to the node. The same address is not added twice. `node` must be a valid ID. |
| `removeSubscription(node, subscriber)` | Removes a subscription by object address. Returns `true` if found, or `false` if the node ID is invalid or the address is absent. |
| `visitMatches(path, subscribers)` | Appends pointers from each existing path prefix to the supplied `customTrie::vector<T*>`. Invalid paths append nothing. If a later segment is missing, earlier matches remain in the result. |

```cpp
#include "team/trie.h"
#include <cstddef>

struct Subscriber { int user_id; };

Subscriber alice{1}; // Must outlive the pointer stored in the trie.
customTrie::trie<Subscriber> topics;
std::size_t news = topics.ensureTopic("news");
topics.addSubscription(news, alice);

customTrie::vector<Subscriber*> matches;
topics.visitMatches("news/sports", matches);
// matches[0] points to alice. Publishing to news does not search its children.
topics.removeSubscription(news, alice);
```

Subscribers are owned by the caller; the trie does not delete them. Remove a subscription before its object is destroyed or moved. `visitMatches` does not clear the result, deduplicate users, or send alerts; the caller handles deduplication and delivery. Removing a subscription scans that node's subscribers. The trie cannot be copied or assigned.

## hashtable: string-to-value mapping

`hashtable<T>` maps string segments to values of type `T`. Keys are passed as a pointer and length; insertion copies the key bytes.

| Function | Behavior |
| --- | --- |
| `hashtable()` | Creates an empty table. |
| `insert(key, len, value)` | Inserts the first `len` characters and their value, or updates the value if the key already exists. Does nothing if `key == nullptr`. |
| `find(key, len, value)` | Writes the value and returns `true` when found; otherwise returns `false`. |
| `size()` | Returns the number of distinct keys. |

```cpp
#include "team/hashtable.h"
#include <cstddef>

customTrie::hashtable<std::size_t> children;
children.insert("sports", 6, 2);
std::size_t id = 0;
if (children.find("sports", 6, id)) {
    // id == 2
}
```

The length must describe the intended key segment and must not exceed the readable memory at `key`. The table resolves collisions with separate chaining. It has no public erase or clear operation and cannot be copied or assigned.

## list: doubly linked list

`list<T>` exposes a `node` type with `value`, `prev`, and `next` fields. The list owns its nodes.

| Function | Behavior |
| --- | --- |
| `list()`, copy constructor, copy assignment | Creates an empty list or copies another list's values. |
| `push_front(value)` / `push_back(value)` | Inserts at the front / back and returns a pointer to the new node. |
| `erase(pos)` | Deletes the given node; `nullptr` does nothing. A non-null pointer must refer to a valid node belonging to this list. |
| `pop_front()` / `pop_back()` | Deletes the first / last node; does nothing on an empty list. |
| `clear()` | Deletes all nodes. |
| `front()` / `back()` | Returns a pointer to the first / last node, or `nullptr` when empty. Const lists return const node pointers. |
| `size()` / `empty()` | Returns the node count / whether the list is empty. |

```cpp
#include "team/list.h"

customTrie::list<int> values;
values.push_front(10);
auto* node = values.push_back(20);
for (auto* p = values.front(); p; p = p->next) {
    // Use p->value.
}
values.erase(node); // node is now invalid.
```

Deleting a node or destroying the list invalidates the affected node pointers. Copying a list creates independent nodes; elements are copied according to `T`'s copy semantics.

## vector: dynamic array

`vector<T>` provides zero-based indexed access and grows automatically.

| Function | Behavior |
| --- | --- |
| `vector()`, copy constructor, copy assignment | Creates an empty array or copies another array's elements. |
| `reserve(capacity)` | Ensures at least the requested capacity without changing the logical size. |
| `push_back(value)` | Appends an element, growing storage if needed. |
| `pop_back()` | Removes one logical element; does nothing when empty. |
| `clear()` | Sets the logical size to zero. |
| `swap(other)` | Exchanges storage and sizes with another vector. |
| `size()` / `empty()` | Returns the logical element count / whether the vector is empty. |
| `operator[](index)` | Returns a reference without bounds checking; `index` must be less than `size()`. |

```cpp
#include "team/vector.h"

customTrie::vector<int> values;
values.reserve(8);
values.push_back(10);
values.push_back(20);
int first = values[0]; // 10
values.pop_back();     // size() == 1
```

Growing storage invalidates pointers and references to existing elements. This implementation allocates with `new T[capacity]`, so `T` must support default construction and copy assignment. `pop_back()` and `clear()` only change the logical size; removed elements are not destroyed immediately. Storage is released when replaced or destroyed.
