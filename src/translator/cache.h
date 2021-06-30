#ifndef SRC_BERGAMOT_CACHE_H_
#define SRC_BERGAMOT_CACHE_H_

#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

/// Thread-safe LRUCache
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LRUCache {
 public:
  struct Stats {
    size_t hits{0};
    size_t misses{0};
  };

  /// Storage includes Key, so when LRU is evicted corresponding hashmap entry can be deleted.
  typedef std::pair<Key, Value> KeyVal;
  typedef typename std::list<KeyVal>::iterator StorageItr;

  LRUCache(size_t sizeCap) : sizeCap_(sizeCap) {}

  /// Bulk inserts keys, values. Only single mutex for multiple sentences.
  void insertBulk(std::vector<Key> &keys, std::vector<Value> &values) {
    std::lock_guard<std::mutex> guard(rwMutex_);
    while (storage_.size() > 0 && sizeCap_ - storage_.size() < keys.size()) {
      unsafeEvict();
    }
    for (size_t idx = 0; idx < keys.size(); idx++) {
      unsafeInsert(keys[idx], values[idx]);
    }
  }

  /// Insert a key, value into cache. Evicts least-recently-used if no space. Thread-safe.
  void insert(const Key key, const Value value) {
    std::lock_guard<std::mutex> guard(rwMutex_);
    if (storage_.size() + 1 > sizeCap_) {
      unsafeEvict();
    }
    unsafeInsert(key, value);
  }

  /// Attempt to fetch a key storing it in value. Returns true if cache-hit, false if cache-miss. Thread-safe.
  bool fetch(const Key key, Value &value) {
    auto mapItr = map_.find(key);
    if (mapItr == map_.end()) {
      ++stats_.misses;
      return false;
    } else {
      ++stats_.hits;

      std::lock_guard<std::mutex> guard(rwMutex_);

      auto storageItr = mapItr->second;
      value = storageItr->second;

      // If fetched, update least-recently-used by moving the element to the front of the list.
      storage_.erase(storageItr);
      unsafeInsert(key, value);
      return true;
    }
  }

  const Stats &stats() const { return stats_; }

 private:
  void unsafeEvict() {
    // Evict LRU
    auto p = storage_.rbegin();
    map_.erase(/*key=*/p->first);
    storage_.pop_back();
  }

  void unsafeInsert(const Key key, const Value value) {
    storage_.push_front({key, value});
    map_.insert({key, storage_.begin()});
  }

  size_t sizeCap_;  /// Number of (key, value) to store at most.

  /// Linked list to keep usage ordering and evict least-recently used.
  std::list<KeyVal> storage_;

  /// hash-map for O(1) cache access. Stores iterator to the list holding cache-elements. Iterators are valid until
  /// they're erased (they don't invalidate on insertion / move of other elements).
  std::unordered_map<Key, StorageItr, Hash> map_;
  std::mutex rwMutex_;  ///< Guards accesses to storage_, map_
  Stats stats_;         ///< Stores hits and misses for log/checks.
};

// Specialize for marian
// This is a lazy hash, we'll fix this with something better later.
struct WordsHashFn {
  size_t operator()(const Words &words) const {
    std::string repr("");
    for (size_t idx = 0; idx < words.size(); idx++) {
      if (idx != 0) {
        repr += " ";
      }
      repr += words[idx].toString();
    }
    return std::hash<std::string>{}(repr);
  }
};

/// For translator, we cache (marian::Words -> History).
typedef LRUCache<Words, History, WordsHashFn> TranslatorLRUCache;

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_CACHE_H_
