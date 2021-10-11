#pragma once
#include <memory>
#include <mutex>
#include <vector>

#include "definitions.h"
#include "translator/history.h"

namespace marian::bergamot {

template <class Key, class Value, class Hash = std::hash<Key>, class Equals = std::equal_to<Key>>
class AtomicCache {
 public:
  using Record = std::pair<Key, Value>;
  struct Stats {
    size_t hits{0};
    size_t misses{0};
    size_t activeRecords{0};
    size_t evictedRecords{0};
    size_t totalSize{9};
  };

  explicit AtomicCache(size_t size, size_t buckets) : records_(size), mutexBuckets_(buckets), used_(size, false) {}

  std::pair<bool, Value> find(const Key &key) const {
    Value value;
    bool found = atomicLoad(key, value);
    return std::make_pair(found, value);
  }

  void store(const Key &key, Value value) { atomicStore(key, value); }

  const Stats stats() const { return stats_; }

 private:
  bool atomicLoad(const Key &key, Value &value) const {
    // No probing, direct map onto records_
    size_t index = hash_(key) % records_.size();
    size_t mutexId = hash_(key) % mutexBuckets_.size();

    std::lock_guard<std::mutex> lock(mutexBuckets_[mutexId]);
    const Record &candidate = records_[index];
    if (key == candidate.first) {
      value = candidate.second;
      stats_.hits += 1;
      stats_.activeRecords += 1;
      return true;
    } else {
      stats_.misses += 1;
    }

    return false;
  }

  void atomicStore(const Key &key, Value value) {
    // No probing, direct map onto records_
    size_t index = hash_(key) % records_.size();
    size_t mutexId = hash_(key) % mutexBuckets_.size();

    std::lock_guard<std::mutex> lock(mutexBuckets_[mutexId]);
    Record &candidate = records_[index];

    if (!used_[index]) {
      stats_.evictedRecords += 1;
      stats_.totalSize += 1;
      used_[index] = true;
    }

    candidate.first = key;
    candidate.second = value;
  }

  std::vector<Record> records_;
  std::vector<bool> used_;

  mutable std::vector<std::mutex> mutexBuckets_;
  mutable Stats stats_;

  Hash hash_;
  Equals equals_;
};

typedef AtomicCache<size_t, Ptr<History>> TranslationCache;

}  // namespace marian::bergamot
