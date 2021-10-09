#pragma once
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

  using FloatingRecord = std::shared_ptr<Record>;

  explicit AtomicCache(std::size_t size) : records_(size, nullptr) {}

  FloatingRecord Find(const Key &key) const {
    const FloatingRecord &record = records_[hash_(key) % records_.size()];
    FloatingRecord ret = std::atomic_load(&record);
    if (ret && equals_(key, ret->key)) {
      return ret;
    } else {
      return FloatingRecord{nullptr};
    }
  }

  void Store(FloatingRecord &recordIn) {
    FloatingRecord &record = records_[hash_(recordIn->key) % records_.size()];
    atomic_store(&record, recordIn);
  }

 private:
  std::vector<FloatingRecord> records_;

  Hash hash_;
  Equals equals_;
};

}  // namespace marian::bergamot
