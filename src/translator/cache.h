#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

/// Thread-safe LRUCache
template <typename CacheItem, typename Key>
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
      return false;
    } else {
      auto storageItr = mapItr->second;
      item = storageItr->second;
      storage_.erase(storageItr);
      storage_.push_front({key, item});
      map_.insert({key, storage_.begin()});
      return true;
    }
  }

 private:
  size_t sizeCap_;
  std::list<StorageItem> storage_;
  std::unordered_map<Key, StorageItr> map_;
  std::mutex rwMutex_;
};

typedef LRUCache<std::string, History> TranslatorLRUCache;

}  // namespace bergamot
}  // namespace marian
