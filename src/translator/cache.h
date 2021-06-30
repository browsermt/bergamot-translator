#ifndef SRC_BERGAMOT_CACHE_H_
#define SRC_BERGAMOT_CACHE_H_

#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

struct LRUCacheStats {
  size_t hits;
  size_t misses;
};

/// Thread-safe LRUCache
template <typename Key, typename CacheItem, typename Hash = std::hash<Key>>
class LRUCache {
 public:
  // Storage includes Key, so when LRU is evicted corresponding hashmap entry can be deleted.
  typedef std::pair<Key, CacheItem> StorageItem;
  typedef typename std::list<StorageItem>::iterator StorageItr;

  LRUCache(size_t sizeCap) : sizeCap_(sizeCap) {}

  void insert(const Key key, const CacheItem item) {
    std::lock_guard<std::mutex> guard(rwMutex_);
    if (storage_.size() + 1 > sizeCap_) {
      // Evict LRU
      auto p = storage_.rbegin();
      map_.erase(/*key=*/p->first);
      storage_.pop_back();
    }

    storage_.push_front({key, item});
    map_.insert({key, storage_.begin()});
  }

  bool fetch(const Key key, CacheItem &item) {
    std::lock_guard<std::mutex> guard(rwMutex_);
    auto mapItr = map_.find(key);
    if (mapItr == map_.end()) {
      ++stats_.misses;
      return false;
    } else {
      ++stats_.hits;
      auto storageItr = mapItr->second;
      item = storageItr->second;
      storage_.erase(storageItr);
      storage_.push_front({key, item});
      map_.insert({key, storage_.begin()});
      return true;
    }
  }

  const LRUCacheStats &stats() const { return stats_; }

 private:
  size_t sizeCap_;
  std::list<StorageItem> storage_;
  std::unordered_map<Key, StorageItr, Hash> map_;
  std::mutex rwMutex_;
  LRUCacheStats stats_;
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

typedef LRUCache<Words, History, WordsHashFn> TranslatorLRUCache;

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_CACHE_H_
