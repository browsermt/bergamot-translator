#pragma once
#include <atomic>
#include <memory>
#include <vector>

namespace marian::bergamot {

template <class Key, class Value, class Hash = std::hash<Key>, class Equals = std::equal_to<Key>>
class AtomicCache {
 public:
  struct Record {
    Key key;
    Value value;
  };

  explicit AtomicCache(std::size_t size) : records_(size) {}

  bool Find(const Key &key, Value &value) const {
    const std::atomic<Record> &record = records_[hash_(key) % records_.size()];
    // Record ret = std::atomic_load(&record);
    Record ret = record.load();
    if (ret.key == key) {
      value = ret.value;
      return true;
    }
    return false;
  }

  void Store(Record &recordIn) {
    std::atomic<Record> &record = records_[hash_(recordIn.key) % records_.size()];
    // std::atomic_store(&record, recordIn);
    record.store(recordIn);
  }

 private:
  std::vector<std::atomic<Record>> records_;

  Hash hash_;
  Equals equals_;
};

}  // namespace marian::bergamot
