#ifndef SRC_BERGAMOT_CACHE_H_
#define SRC_BERGAMOT_CACHE_H_

#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#ifndef WASM_COMPATIBLE_SOURCE
#include "3rd_party/L4/inc/L4/LocalMemory/HashTableService.h"
#endif

#include "processed_request_sentence.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

namespace cache_util {

// Provides a unique representation of marian::Words as a string
std::string wordsToString(const marian::Words &words);

struct HashWords {
  size_t operator()(const Words &words) const { return std::hash<std::string>{}(wordsToString(words)); }
};

}  // namespace cache_util

struct CacheStats {
  size_t hits{0};
  size_t misses{0};
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
/// unsuitable for WASM's current blocking workflow.
///
/// Keys are marian::Words, values are ProcessedRequestSentences.
///
/// There is a serialization (converting to binary (data*, size)) of structs overhead with this class as well, which
/// helps keep tight lid on memory usage. This should however be cheaper in comparison to recomputing through the graph.

#ifndef WASM_COMPATIBLE_SOURCE
class ThreadSafeL4Cache {
 public:
  using KeyBytes = L4::IWritableHashTable::Key;
  using ValueBytes = L4::IWritableHashTable::Value;

  /// Construct a ThreadSafeL4Cache from configuration passed via key values in an Options object:
  ///
  /// @param [in] options: Options object which is used to configure the cache. See command-line parser for
  /// documentation (`marian::bergamot::createConfigParser()`)
  ThreadSafeL4Cache(Ptr<Options> options);

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
  void debug(std::string) const;

  /// cacheConfig {sizeInBytes, recordTimeToLive, removeExpired} determins the upper limit on cache storage, time for a
  /// record to live and whether or not to evict expired records.
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
};

#endif

/// Alternative cache for non-thread based workflow (specifically WASM). LRU Eviction Policy. Uses a lot of std::list.
/// Yes, std::list; If someone needs a more efficient version, look into threads and go for ThreadSafeL4Cache.
/// This class comes as an afterthought, so expect parameters which may not be used to achieve parity with
/// ThreadSafeL4Cache.

class ThreadUnsafeLRUCache {
 public:
  /// Construct a ThreadUnSafeLRUCache from configuration passed via key values in an Options object:
  ///
  /// @param [in] options: Options object which is used to configure the cache. See command-line parser for
  /// documentation (`marian::bergamot::createConfigParser()`)
  ThreadUnsafeLRUCache(Ptr<Options> options);

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
  struct Record {
    marian::Words key;
    std::string value;  // serialized representation from ProcessedRequestSentence.toBytes()
    size_t size() const { return key.size() * sizeof(marian::Word) + value.size() * sizeof(std::string::value_type); }
  };

  // A list representing memory where the items are stored and a hashmap (unordered_map) with values pointing to this
  // storage is used for a naive LRU implementation.
  std::list<Record> storage_;
  typedef std::list<Record>::iterator RecordPtr;
  std::unordered_map<marian::Words, RecordPtr, cache_util::HashWords> cache_;

  /// Active sizes (in bytes) stored in storage_
  size_t storageSize_;

  // Limit of size (in bytes) of storage_
  size_t storageSizeLimit_;

  CacheStats stats_;
};

#ifndef WASM_COMPATIBLE_SOURCE
typedef ThreadSafeL4Cache TranslationCache;
#else
typedef ThreadUnsafeLRUCache TranslationCache;
#endif

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_CACHE_H_
