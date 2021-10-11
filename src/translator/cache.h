#pragma once
#include <memory>
#include <mutex>
#include <vector>

namespace marian::bergamot {

template <class Key, class Value, class Hash = std::hash<Key>, class Equals = std::equal_to<Key>>
class AtomicCache {
 public:
  using Record = std::pair<Key, Value>;

  explicit AtomicCache(size_t size, size_t buckets) : records_(size), mutexBuckets_(buckets) {}

  std::pair<bool, Value> Find(const Key &key) const {
    Value value;
    bool found = AtomicLoad(key, value);
    return std::make_pair(found, value);
  }

  void Store(const Key &key, Value &&value) { AtomicStore(key, std::move(value)); }

 private:
  bool AtomicLoad(const Key &key, Value &value) const {
    // No probing, direct map onto records_
    size_t index = hash_(key) % records_.size();
    size_t mutexId = hash_(key) % mutexBuckets_.size();

    std::lock_guard<std::mutex> lock(mutexBuckets_[mutexId]);
    const Record &candidate = records_[index];
    if (key == candidate.first) {
      value = candidate.second;
      return true;
    }

    return false;
  }

  void AtomicStore(const Key &key, Value &&value) {
    // No probing, direct map onto records_
    size_t index = hash_(key) % records_.size();
    size_t mutexId = hash_(key) % mutexBuckets_.size();

    std::lock_guard<std::mutex> lock(mutexBuckets_[mutexId]);
    Record &candidate = records_[index];

    candidate.first = key;
    candidate.second = std::move(value);
  }

  std::vector<Record> records_;
  mutable std::vector<std::mutex> mutexBuckets_;

  Hash hash_;
  Equals equals_;
};

}  // namespace marian::bergamot
