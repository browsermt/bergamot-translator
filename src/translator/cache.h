#ifndef SRC_BERGAMOT_CACHE_H_
#define SRC_BERGAMOT_CACHE_H_

#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#ifndef WASM_COMPATIBLE_SOURCE
#include "3rd_party/L4/inc/L4/LocalMemory/HashTableService.h"
#endif

#include "common/hash.h"
#include "processed_request_sentence.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

namespace cache_util {

template <class HashWordType>
struct HashWords {
  HashWordType operator()(const Words &words) const {
    size_t seed = 42;
    for (auto &word : words) {
      HashWordType hashWord = static_cast<HashWordType>(word.toWordIndex());
      util::hash_combine<HashWordType>(seed, hashWord);
    }
    return seed;
  }
};

}  // namespace cache_util

struct CacheConfig {
  size_t size;
  size_t ebrQueueSize;
  size_t ebrNumQueues;
  size_t ebrIntervalInMilliseconds;
  size_t numBuckets;
  bool removeExpired;
  bool timeToLiveInMilliseconds;
};

struct CacheStats {
  size_t hits{0};
  size_t misses{0};
  size_t activeRecords{0};
  size_t evictedRecords{0};
  size_t totalSize{0};
};

class TranslationCache {
 public:
  virtual bool fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence) = 0;
  virtual void insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence) = 0;
  virtual CacheStats stats() const = 0;
};

/// ThreadSafeL4Cache is an adapter built on top of L4 specialized for the use-case of bergamot-translator. L4 is a
/// thread-safe cache implementation written in the "lock-free" paradigm using atomic constucts. There is locking when
/// structures are deleted, which runs in the background using an epoch based memory reclamation (EBR) scheme. Eviction
/// follows the "Clock" algorithm, which is a practical approximation for LRU. Records live either until they're used at
/// least once, or if there is no space left without removing one that's not been used yet, with removal triggered to
/// make space for new insertions.
///
/// This implementation creates one additional thread which manages the garbage collection for the class.  While it may
/// be possible to get this class to compile in WASM architecture, the additional thread makes using this class
/// unsuitable for WASM's current blocking workflow.  Keys are marian::Words, values are ProcessedRequestSentences.
///
/// There is a serialization (converting to binary (data*, size)) of structs overhead with this class as well, which
/// helps keep tight lid on memory usage. This should however be cheaper in comparison to recomputing through the graph.

#ifndef WASM_COMPATIBLE_SOURCE
class ThreadSafeL4Cache : public TranslationCache {
 public:
  // L4 has weird interfaces with Hungarian Notation (hence IWritableHashTable). All implementations take Key and Value
  // defined in this interface. Both Key and Value are of format: (uint8* mdata, size_t size). L4 doesn't own these - we
  // point something that is alive for the duration, the contents of this are memcpy'd into L4s internal storage.
  //
  // To hide the cryptic names, this class uses friendly "KeyBytes" and "ValueBytes", which is what they essentially
  // are.
  using KeyBytes = L4::IWritableHashTable::Key;
  using ValueBytes = L4::IWritableHashTable::Value;

  /// Construct a ThreadSafeL4Cache from configuration passed via key values in an Options object:
  ///
  /// @param [in] options: Options object which is used to configure the cache. See command-line parser for
  /// documentation (`marian::bergamot::createConfigParser()`)
  ThreadSafeL4Cache(const CacheConfig &config);

  /// Fetches a record from cache, storing it in processedRequestSentence if found. Calls to fetch are thread-safe and
  /// lock-free.
  ///
  /// @param [in]  words: query marian:Words for which translation results are looked up in cache.
  /// @param [out] processedRequestSentence: stores cached results for use outside if found.
  ///
  /// @returns true if query found in cache false otherwise.
  bool fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence);

  /// Inserts a new record into cache. Thread-safe. Modifies the structure so takes locks, configure sharding to
  /// configure how fine-grained the locks are to reduce contention.
  ///
  /// @param [in] words: marian::Words processed from a sentence
  /// @param [in] processedRequestSentence: minimum translated information corresponding to words
  ///
  void insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence);

  CacheStats stats() const;

 private:
  /// cacheConfig {sizeInBytes, recordTimeToLive, removeExpired} determins the upper limit on cache storage, time for
  /// a record to live and whether or not to evict expired records.
  L4::HashTableConfig::Cache cacheConfig_;

  /// epochManagerConfig_{epochQueueSize, epochProcessingInterval, numActionQueues}:
  /// - epochQueueSize (How many actions to keep in a Queue)
  /// - epochProcessingInterval (Reclaimation runs in a background thread. The interval at which this thread is run).
  /// - numActionQueues (Possibly to increase throughput of registering actions by a finer-granularity of locking).
  /// .. and "action" is an std::function<void(void)>..., which is run by the background thread.
  L4::EpochManagerConfig epochManagerConfig_;

  /// L4 Service. There's a string key based registration through service_ and accesses from multiple processes to get
  /// the same map.
  L4::LocalMemory::HashTableService service_;

  /// An L4 Context, does magic with epochs everytime accessed;  "Once a context is retrieved, the operations such as
  /// operator[] on the context and Get() are lock-free."
  L4::LocalMemory::Context context_;

  /// context_[hashTableIndex_] gives the hashmap for Get(...) or Add(...) operations
  size_t hashTableIndex_;

  cache_util::HashWords<uint64_t> hashFn_;
};

#endif

/// Alternative cache for non-thread based workflow (specifically WASM). LRU Eviction Policy. Uses a lot of std::list.
/// Yes, std::list; If someone needs a more efficient version, look into threads and go for ThreadSafeL4Cache.
/// This class comes as an afterthought, so expect parameters which may not be used to achieve parity with
/// ThreadSafeL4Cache.

class ThreadUnsafeLRUCache : public TranslationCache {
 public:
  /// Construct a ThreadUnSafeLRUCache from configuration passed via key values in an Options object:
  ///
  /// @param [in] options: Options object which is used to configure the cache. See command-line parser for
  /// documentation (`marian::bergamot::createConfigParser()`)
  ThreadUnsafeLRUCache(const CacheConfig &config);

  /// Fetches a record from cache, storing it in processedRequestSentence if found. Not thread-safe.
  ///
  /// @param [in]  words: query marian:Words for which translation results are looked up in cache.
  /// @param [out] processedRequestSentence: stores cached results for use outside if found.
  ///
  /// @returns true if query found in cache false otherwise.
  bool fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence);

  /// Inserts a new record into cache.
  ///
  /// @param [in] words: marian::Words processed from a sentence
  /// @param [in] processedRequestSentence: minimum translated information corresponding to words
  void insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence);

  CacheStats stats() const;

 private:
  // A Record type holds Key and Value together in a linked-list as a building block for the LRU cache.
  // Currently we use uint64_t for Key to reduce collisions. Value (Storage) is simply a binary sequential memory with
  // some enhancements to read variables and ranges from the memory. The API works with marian::Words (internally
  // reduced to uint6_t to reduce vector comparisons) and ProcessedRequestSentence (which can be constructed given a
  // Storage - which is a more compressed representation).
  struct Record {
    using Key = std::uint64_t;
    using Value = Storage;

    Key key;
    Value value;

    size_t size() const { return /*key=*/sizeof(Key) + value.size(); }
  };

  // A list representing memory where the items are stored and a hashmap (unordered_map) with values pointing to this
  // storage is used for a naive LRU implementation.
  using RecordList = std::list<Record>;
  using RecordPtr = RecordList::iterator;

  RecordList storage_;
  std::unordered_map<Record::Key, RecordPtr> cache_;

  /// Active sizes (in bytes) stored in storage_
  size_t storageSize_;

  // Limit of size (in bytes) of storage_
  size_t storageSizeLimit_;

  cache_util::HashWords<Record::Key> hashFn_;

  CacheStats stats_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_CACHE_H_
