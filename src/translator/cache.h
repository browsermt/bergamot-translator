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
  LRUCache(size_t sizeCap) : sizeCap_(sizeCap) {}

  /// Insert a key, value into cache. Evicts least-recently-used if no space. Thread-safe.
  void insert(const Key key, const Value value);

  /// Attempt to fetch a key storing it in value. Returns true if cache-hit, false if cache-miss. Thread-safe.
  bool fetch(const Key key, Value &value);

  CacheStats stats() const { return stats_; }

 private:
  struct Record {
    Key key;
    Value value;
  };

  typedef typename std::list<Record>::iterator RecordIterator;

  void unsafeEvict();
  void unsafeInsert(const Key key, const Value value);

  size_t sizeCap_;  /// Number of (key, value) to store at most.

  /// Linked list to keep usage ordering and evict least-recently used.
  std::list<Record> storage_;

  /// hash-map for O(1) cache access. Stores iterator to the list holding cache-elements. Iterators are valid until
  /// they're erased (they don't invalidate on insertion / move of other elements).
  std::unordered_map<Key, RecordIterator, Hash> map_;
  std::mutex rwMutex_;  ///< Guards accesses to storage_, map_
  CacheStats stats_;    ///< Stores hits and misses for log/checks.
};

// Specialize for marian
// This is a lazy hash, we'll fix this with something better later.
struct WordsHashFn {
  size_t operator()(const Words &words) const;
};

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
  ProcessedRequestSentence();

  /// Construct from History
  ProcessedRequestSentence(const History &history);
  void debug();

  // Const accessors for private members
  const marian::Words &words() const { return words_; }
  const data::SoftAlignment &softAlignment() const { return softAlignment_; }
  const std::vector<float> &wordScores() const { return wordScores_; }
  float sentenceScore() const { return sentenceScore_; }

  static ProcessedRequestSentence fromBytes(char const *data, size_t size);
  std::string toBytes() const;

 private:
  marian::Words words_;                ///< vector of marian::Word, no shared_ptrs
  data::SoftAlignment softAlignment_;  ///< history->traceSoftAlignment(); std::vector<std::vector<float>>
  std::vector<float> wordScores_;      ///< hyp->tracebackWordScores();
  float sentenceScore_;                ///< normalizedPathScore
};

/// LocklessCache Adapter built on top of L4

class LockLessClockCache {
 public:
  using KeyBytes = L4::IWritableHashTable::Key;
  using ValueBytes = L4::IWritableHashTable::Value;

  using Key = marian::Words;
  using Value = marian::bergamot::ProcessedRequestSentence;

  LockLessClockCache(const std::string &modelIdentifier, size_t sizeInBytes, size_t timeOutInSeconds,
                     bool removeExpired = true);
  bool fetch(const Key &key, Value &value);
  void insert(const Key &key, const Value &value);
  CacheStats stats() const;

 private:
  KeyBytes keyToBytes(const Key &key);

  L4::HashTableConfig::Cache cacheConfig_;
  L4::EpochManagerConfig epochManagerConfig_;
  L4::LocalMemory::HashTableService service_;
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
