#ifndef SRC_BERGAMOT_CACHE_H_
#define SRC_BERGAMOT_CACHE_H_

#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#ifndef WASM_COMPATIBLE_SOURCE
#include "3rd_party/L4/inc/L4/LocalMemory/HashTableService.h"
#endif

#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

namespace cache_util {

// Provides a unique representation of marian::Words as a string
inline std::string wordsToString(const marian::Words &words) {
  std::string repr;
  for (size_t i = 0; i < words.size(); i++) {
    if (i != 0) {
      repr += " ";
    }
    repr += words[i].toString();
  }
  return repr;
}

struct HashWords {
  size_t operator()(const Words &words) const { return std::hash<std::string>{}(wordsToString(words)); }
};

}  // namespace cache_util

struct CacheStats {
  size_t hits{0};
  size_t misses{0};
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
  ProcessedRequestSentence();

  /// Construct from History - consolidate members which we require further and store the complete object (not
  /// references to shared-pointers) for storage and recompute efficiency purposes.
  ProcessedRequestSentence(const History &history);
  void debug();

  // Const accessors for private members
  const marian::Words &words() const { return words_; }
  const data::SoftAlignment &softAlignment() const { return softAlignment_; }
  const std::vector<float> &wordScores() const { return wordScores_; }
  float sentenceScore() const { return sentenceScore_; }

  // Helpers to work with blobs / bytearray storing the class, particularly for L4. Storage follows the order of member
  // definitions in this class. With vectors prefixed with sizes to allocate before reading in with the right sizes.

  /// Deserialize the contents of an instance from a sequence of bytes. Compatible with toBytes.
  static ProcessedRequestSentence fromBytes(char const *data, size_t size);

  /// Serialize the contents of an instance to a sequence of bytes. Should be compatible with fromBytes.
  std::string toBytes() const;

 private:
  // Note: Adjust blob IO in order of the member definitions here, in the event of change.

  marian::Words words_;                ///< vector of marian::Word, no shared_ptrs
  data::SoftAlignment softAlignment_;  ///< history->traceSoftAlignment(); std::vector<std::vector<float>>
  std::vector<float> wordScores_;      ///< hyp->tracebackWordScores();
  float sentenceScore_;                ///< normalizedPathScore
};

/// LocklessCache Adapter built on top of L4

#ifndef WASM_COMPATIBLE_SOURCE
class LockLessClockCache {
 public:
  using KeyBytes = L4::IWritableHashTable::Key;
  using ValueBytes = L4::IWritableHashTable::Value;

  LockLessClockCache(size_t sizeInBytes, size_t timeOutInSeconds, bool removeExpired = false);
  bool fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence);
  void insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence);
  CacheStats stats() const;

 private:
  void debug(std::string) const;
  std::string wordsToString(const marian::Words &words) { return cache_util::wordsToString(words); };

  L4::HashTableConfig::Cache cacheConfig_;
  L4::EpochManagerConfig epochManagerConfig_;
  L4::LocalMemory::HashTableService service_;
  L4::LocalMemory::Context context_;
  size_t hashTableIndex_;
};

#endif

/// Alternative cache for non-thread based workflow. Clock Eviction Policy. Uses a lot of std::list
/// Yes, std::list; If you need a more efficient version, look into threads and go for LockLessClockCache.

class ThreadUnsafeCache {
 public:
  ThreadUnsafeCache(size_t sizeInBytes, size_t timeOutInSeconds, bool removeExpired = false);
  bool fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence);
  void insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence);
  CacheStats stats() const;

 private:
  struct Record {
    marian::Words key;
    std::string value;
    size_t size() const { return key.size() * sizeof(marian::Word) + value.size() * sizeof(std::string::value_type); }
  };

  std::list<Record> storage_;
  typedef std::list<Record>::iterator RecordPtr;
  std::unordered_map<marian::Words, RecordPtr, cache_util::HashWords> cache_;

  size_t storageSize_;
  size_t storageSizeLimit_;

  CacheStats stats_;
};

typedef std::vector<std::unique_ptr<ProcessedRequestSentence>> ProcessedRequestSentences;

#ifndef WASM_COMPATIBLE_SOURCE
typedef LockLessClockCache TranslatorLRUCache;
#else
typedef ThreadUnsafeCache TranslatorLRUCache;
#endif

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_CACHE_H_
