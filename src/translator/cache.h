#ifndef SRC_BERGAMOT_CACHE_H_
#define SRC_BERGAMOT_CACHE_H_

#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "3rd_party/L4/inc/L4/LocalMemory/HashTableService.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

struct CacheStats {
  size_t hits{0};
  size_t misses{0};
};

/// Thread-safe LRUCache
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LRUCache {
 public:
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
    std::lock_guard<std::mutex> guard(rwMutex_);
    auto mapItr = map_.find(key);
    if (mapItr == map_.end()) {
      ++stats_.misses;
      return false;
    } else {
      ++stats_.hits;

      auto storageItr = mapItr->second;
      value = storageItr->second;

      // If fetched, update least-recently-used by moving the element to the front of the list.
      storage_.erase(storageItr);
      unsafeInsert(key, value);
      return true;
    }
  }

  CacheStats stats() const { return stats_; }

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
  CacheStats stats_;    ///< Stores hits and misses for log/checks.
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

namespace {

// Read write into Bytes for ProcessedRequestSentence

// Write Vector
template <class T>
void writeVector(std::ostream &out, const std::vector<T> &t) {
  const char *data = reinterpret_cast<const char *>(t.data());
  size_t size = sizeof(t) * t.size();
  out << t.size();
  out << string_view(data, size);
}

// Read Vector
template <class T>
void readVector(std::istream &in, std::vector<T> &t) {
  size_t size;
  in >> size;
  t.reserve(size);

  char *data = reinterpret_cast<char *>(t.data());
  in.read(data, size * sizeof(T));
}

}  // namespace

/// History is marian object with plenty of shared_ptrs lying around, which makes keeping a lid on memory hard for
/// cache, due to copies being shallow rather than deep. We therefore process marian's lazy History management changing
/// to something eager, keeping only what we want and units accounted for. Each of these corresponds to a
/// `RequestSentence` and retains the minimum information required to reconstruct a full Response if saved in Cache.
///
/// L4 requires byte-streams, if we are to keep tight control over "memory" we need to have accounting for cache-size
/// based on memory/bytes as well.
///
/// If the layout of this struct changes, we should be able to use version mismatches to invalidate cache to avoid
/// false positives, if the cache ever makes it to disk / external storage. While this is desirable, for now this is
/// in-memory.

class ProcessedRequestSentence {
 public:
  /// Empty construction
  ProcessedRequestSentence() {}

  /// Construct from History
  ProcessedRequestSentence(const History &history) {
    IPtr<Hypothesis> hypothesis;
    Result result = history.top();
    std::tie(words_, hypothesis, sentenceScore_) = result;
    softAlignment_ = hypothesis->tracebackAlignment();
    wordScores_ = hypothesis->tracebackWordScores();
  }

  /// Construct from stream of bytes
  ProcessedRequestSentence(std::istream &stream) {
    readVector<marian::Word>(stream, words_);
    size_t numAlignments;
    stream >> numAlignments;
    softAlignment_.resize(numAlignments);
    for (size_t i = 0; i < numAlignments; i++) {
      readVector<float>(stream, softAlignment_[i]);
    }

    readVector<float>(stream, wordScores_);
    stream >> sentenceScore_;
  }

  std::string bytes() const {
    // Format is type(count, bytes[count*sizeof(type)];
    // Convention is to maintain this in order of how the members are defined;
    // Types are hardcoded.
    //
    // TODO(@jerinphilip): Probably inefficient because everyone hates C++ streams, but let's wire L4 first and fix this
    // once that's correct.
    //
    std::stringstream stream;
    writeVector<marian::Word>(stream, words_);

    stream << softAlignment_.size();
    for (auto &v : softAlignment_) {
      writeVector<float>(stream, v);
    }

    writeVector<float>(stream, wordScores_);
    stream << sentenceScore_;
    return stream.str();
  }

  // Const accessors for private members
  const marian::Words &words() const { return words_; }
  const data::SoftAlignment &softAlignment() const { return softAlignment_; }
  const std::vector<float> &wordScores() const { return wordScores_; }
  float sentenceScore() const { return sentenceScore_; }

 private:
  marian::Words words_;                ///< vector of marian::Word, no shared_ptrs
  data::SoftAlignment softAlignment_;  ///< history->traceSoftAlignment(); std::vector<std::vector<float>>
  std::vector<float> wordScores_;      ///< hyp->tracebackWordScores();
  float sentenceScore_;                ///< normalizedPathScore
};

/// LocklessCache Adapter built on top of L4

class LockLessClockCache {
 public:
  using HashTableConfig = L4::HashTableConfig;
  using HashTableService = L4::LocalMemory::HashTableService;

  using KeyBytes = L4::IWritableHashTable::Key;
  using ValueBytes = L4::IWritableHashTable::Value;

  using Key = marian::Words;
  using Value = marian::bergamot::ProcessedRequestSentence;

  LockLessClockCache(const std::string &modelIdentifier, size_t sizeInBytes, size_t timeOutInSeconds,
                     bool removeExpired = true)
      : cacheConfig_{sizeInBytes, std::chrono::seconds(timeOutInSeconds), removeExpired},
        modelIdentifier_(modelIdentifier),
        context_(service_.GetContext()) {
    // Once a context is retrieved, the operations such as
    // operator[] on the context and Get() are lock-free.
    hashTableIndex_ = service_.AddHashTable(
        HashTableConfig(modelIdentifier_, HashTableConfig::Setting{/*numBuckets=*/1000000}, cacheConfig_));
  }

  bool fetch(const Key key, Value &value) {
    auto &hashTable = context_[hashTableIndex_];
    ValueBytes valBytes;
    bool fetchSuccess = hashTable.Get(keyToBytes(key), valBytes);

    if (fetchSuccess) {
      std::string str(reinterpret_cast<const char *>(valBytes.m_data), valBytes.m_size);
      std::stringstream istream(str);
      value = Value(istream);
    }

    return fetchSuccess;
  }

  void insert(const Key key, const Value value) {
    auto &hashTable = context_[hashTableIndex_];
    hashTable.Add(keyToBytes(key), valueToBytes(value));
  }

  KeyBytes keyToBytes(const Key &key) {
    KeyBytes keyBytes;
    keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(key.data());
    keyBytes.m_size = sizeof(Key) * key.size();
    return keyBytes;
  }

  ValueBytes valueToBytes(const Value &value) {
    ValueBytes valBytes;
    std::string serialized = value.bytes();
    valBytes.m_data = reinterpret_cast<const std::uint8_t *>(serialized.data());
    valBytes.m_size = serialized.size();
    return valBytes;
  }

  CacheStats stats() const {
    auto &perfData = context_[hashTableIndex_].GetPerfData();
    CacheStats stats;
    stats.hits = perfData.Get(L4::HashTablePerfCounter::CacheHitCount);
    stats.misses = perfData.Get(L4::HashTablePerfCounter::CacheMissCount);
    return stats;
  };

 private:
  HashTableConfig::Cache cacheConfig_;
  HashTableService service_;
  L4::LocalMemory::Context context_;
  size_t hashTableIndex_;
  const std::string modelIdentifier_;
};

typedef std::vector<std::unique_ptr<ProcessedRequestSentence>> ProcessedRequestSentences;
/// For translator, we cache (marian::Words -> ProcessedRequestSentence).
// typedef LRUCache<Words, ProcessedRequestSentence, WordsHashFn> TranslatorLRUCache;
typedef LockLessClockCache TranslatorLRUCache;

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_CACHE_H_
