#ifndef SRC_BERGAMOT_CACHE_H_
#define SRC_BERGAMOT_CACHE_H_

#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "3rd_party/L4/inc/L4/LocalMemory/HashTableService.h"
#include "common/hash.h"
#include "processed_request_sentence.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

class TranslationModel;

/// Interface for for a cache which caches previously computed translation results for a query uniquely identified by
/// (TranslationModel, marian::Words). This allows the calls to cache within Request for each RequestSentence to be
/// agnostic to the underlying Cache implementation, so long as they implement this interface.
///
/// [Q] Why not diverge WASM and Native if there are so much diverging differences?
/// A: Unfortunately cache is part of common-code used by translation for both WASM and Native. Solutions here are
/// interface inheritance at runtime or swapping classes at compile-time. This implementation choses to go for interface
/// inheritance, abstracting common structures into the interface below, in addition to specifying the contract required
/// for translation to work.

class TranslationCache {
 public:
  /// Common stats structure between both caches.
  struct Stats {
    size_t hits{0};
    size_t misses{0};
    size_t activeRecords{0};
    size_t evictedRecords{0};
    size_t totalSize{0};
  };

 protected:
  /// The key for cache is (model, words). This is a thin record type. We only require these to be references valid to
  /// be able to dereference at the time of hashing, hence we only store the references. It is on the user to ensure
  /// that the addresses these point to don't become invalid during process.
  struct CacheKey {
    const TranslationModel &model;
    const marian::Words &words;
  };

  /// Hashes the marian::Words salted by a unique-id picked up from the model.
  static size_t hash(const TranslationCache::CacheKey &key);

 public:
  // Interface.
  //
  /// @param[in]  CacheKey = (TranslationModel&, Words&) for which previously computed translation results are looked
  /// up in cache.
  /// @param[out] processedRequestSentence: stores cached results for use outside if found.
  ///
  /// @returns true if query found in cache false otherwise.
  virtual bool fetch(const CacheKey &cacheKey, ProcessedRequestSentence &processedRequestSentence) = 0;

  virtual void insert(const CacheKey &cacheKey, const ProcessedRequestSentence &processedRequestSentence) = 0;
  virtual TranslationCache::Stats stats() const = 0;
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

class ThreadSafeL4Cache : public TranslationCache {
 public:
  /// Construct a ThreadSafeL4Cache from configuration specified by TranslationCache::Config
  struct Config {
    size_t sizeInMB{20};
    size_t ebrQueueSize{1000};
    size_t ebrNumQueues{4};
    size_t ebrIntervalInMilliseconds{1000 /*ms*/};
    size_t numBuckets{10000};
    bool removeExpired{false};
    size_t timeToLiveInMilliseconds{1000};

    template <class App>
    static void addOptions(App &app, Config &config) {
      app.add_option("--cache-size", config.sizeInMB, "Megabytes of storage used by cache");
      app.add_option("--cache-ebr-queue-size", config.ebrQueueSize,
                     "Number of action to allow in a queue for epoch based reclamation.");
      app.add_option("--cache-ebr-num-queues", config.ebrNumQueues,
                     "Number of queues of actions to maintain, increase to increase throughput.");
      app.add_option("--cache-ebr-interval", config.ebrIntervalInMilliseconds,
                     "Time between runs of background thread for epoch-based-reclamation, in milliseconds.");
      app.add_option("--cache-buckets", config.numBuckets, "Number of buckets to keep in the hashtable");
      app.add_option("--cache-remove-expired", config.removeExpired,
                     "Whether to remove expired records on a garbage collection iteration or not. Expiry determined by "
                     "user-specified time-window");
      app.add_option("--cache-time-to-live", config.timeToLiveInMilliseconds,
                     "How long a record in cache should be valid for in milliseconds. This is insignificant without "
                     "remove expired flag set.");
    }
  };

  ThreadSafeL4Cache(const ThreadSafeL4Cache::Config &config);

  // L4 has weird interfaces with Hungarian Notation (hence IWritableHashTable). All implementations take Key and Value
  // defined in this interface. Both Key and Value are of format: (uint8* mdata, size_t size). L4 doesn't own these - we
  // point something that is alive for the duration, the contents of this are memcpy'd into L4s internal storage.
  //
  // To hide the cryptic names, this class uses friendly "KeyBytes" and "ValueBytes", which is what they essentially
  // are.
  using KeyBytes = L4::IWritableHashTable::Key;
  using ValueBytes = L4::IWritableHashTable::Value;

  /// Fetches a record from cache, storing it in processedRequestSentence if found. Calls to fetch are thread-safe and
  /// lock-free.
  bool fetch(const CacheKey &cacheKey, ProcessedRequestSentence &processedRequestSentence) override;

  /// Inserts a new record into cache. Thread-safe. Modifies the structure so takes locks, configure sharding to
  /// configure how fine-grained the locks are to reduce contention.
  void insert(const CacheKey &cacheKey, const ProcessedRequestSentence &processedRequestSentence) override;

  TranslationCache::Stats stats() const;

  ThreadSafeL4Cache(const ThreadSafeL4Cache &) = delete;
  ThreadSafeL4Cache &operator=(const ThreadSafeL4Cache &) = delete;

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
};

/// Alternative cache for non-thread based workflow (specifically WASM). LRU Eviction Policy. Uses a lot of std::list.
/// Yes, std::list; If someone needs a more efficient version, look into threads and go for ThreadSafeL4Cache.
/// This class comes as an afterthought, so expect parameters which may not be used to achieve parity with
/// ThreadSafeL4Cache.
class ThreadUnsafeLRUCache : public TranslationCache {
 public:
  struct Config {
    size_t sizeInMB;
    template <class App>
    static void addOptions(App &app, Config &config) {
      app.add_option("--cache-size", config.sizeInMB, "Megabytes of storage used by cache");
    }
  };

  ThreadUnsafeLRUCache(const ThreadUnsafeLRUCache::Config &config);
  bool fetch(const CacheKey &cacheKey, ProcessedRequestSentence &processedRequestSentence) override;
  void insert(const CacheKey &cacheKey, const ProcessedRequestSentence &processedRequestSentence) override;
  TranslationCache::Stats stats() const;

 private:
  // A Record type holds Key and Value together in a linked-list as a building block for the LRU cache.
  // Currently we use uint64_t for Key to reduce collisions. Value (Storage) is simply a binary sequential memory with
  // some enhancements to read variables and ranges from the memory. The API works with marian::Words (internally
  // reduced to uint6_t to reduce vector comparisons) and ProcessedRequestSentence (which can be constructed given a
  // Storage - which is a more compressed representation).
  struct Record {
    using Key = size_t;
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

  TranslationCache::Stats stats_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_CACHE_H_
