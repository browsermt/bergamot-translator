#include "cache.h"

#include <cstdlib>

#include "translation_model.h"

namespace marian {
namespace bergamot {

size_t TranslationCache::hash(const TranslationCache::CacheKey &key) {
  size_t seed = 42;
  for (auto &word : key.words) {
    size_t hashWord = static_cast<size_t>(word.toWordIndex());
    util::hash_combine<size_t>(seed, hashWord);
  }

  util::hash_combine<size_t>(seed, key.model.modelId());
  return seed;
}

ThreadSafeL4Cache::ThreadSafeL4Cache(const ThreadSafeL4Cache::Config &config)
    : epochManagerConfig_(config.ebrQueueSize, std::chrono::milliseconds(config.ebrIntervalInMilliseconds),
                          config.ebrNumQueues),
      cacheConfig_(config.sizeInMB * 1024 * 1024, std::chrono::seconds(config.timeToLiveInMilliseconds),
                   config.removeExpired),
      service_(epochManagerConfig_),
      hashTableIndex_(service_.AddHashTable(L4::HashTableConfig(
          "global-cache", L4::HashTableConfig::Setting{static_cast<uint32_t>(config.numBuckets)}, cacheConfig_)))

{}

bool ThreadSafeL4Cache::fetch(const CacheKey &cacheKey, ProcessedRequestSentence &processedRequestSentence) {
  auto context = service_.GetContext();
  auto &hashTable = context[hashTableIndex_];

  /// We take marian's default hash function, which is templated for hash-type. We treat marian::Word (which are indices
  /// represented by size_t (unverified) and convert it into a size_t for hashing with more bits)
  ///
  /// Note that there can still be collisions, in which case the translations returned from cache can be wrong. It's
  /// practically very unlikely (claims @XapaJIaMnu). To guarantee correctness, comparisons need to be made with the
  /// actual key, which is cannot be done with this approach, as the cache is unaware of the original marian::Words key.
  //
  /// TODO(@jerinphilip): Empirical evaluation that this is okay.

  KeyBytes keyBytes;
  size_t key = TranslationCache::hash(cacheKey);

  // L4 requires uint8_t byte-stream, so we treat 64 bits as a uint8 array, with 8 members.
  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(&key);
  keyBytes.m_size = 8;

  ValueBytes valBytes;
  bool fetchSuccess = hashTable.Get(keyBytes, valBytes);
  if (fetchSuccess) {
    const char *data = reinterpret_cast<const char *>(valBytes.m_data);
    size_t size = valBytes.m_size;
    string_view bytesView(data, size);
    processedRequestSentence = std::move(ProcessedRequestSentence::fromBytesView(bytesView));
  }

  return fetchSuccess;
}

void ThreadSafeL4Cache::insert(const CacheKey &cacheKey, const ProcessedRequestSentence &processedRequestSentence) {
  auto context = service_.GetContext();
  auto &hashTable = context[hashTableIndex_];

  KeyBytes keyBytes;

  size_t key = hash(cacheKey);

  // L4 requires uint8_t byte-stream, so we treat 64 bits as a uint8 array, with 8 members.
  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(&key);
  keyBytes.m_size = 8;

  ValueBytes valBytes;
  string_view serialized = processedRequestSentence.toBytesView();

  valBytes.m_data = reinterpret_cast<const std::uint8_t *>(serialized.data());
  valBytes.m_size = sizeof(string_view::value_type) * serialized.size();

  hashTable.Add(keyBytes, valBytes);
}

TranslationCache::Stats ThreadSafeL4Cache::stats() {
  auto context = service_.GetContext();
  auto &perfData = context[hashTableIndex_].GetPerfData();
  TranslationCache::Stats stats;
  stats.hits = perfData.Get(L4::HashTablePerfCounter::CacheHitCount);
  stats.misses = perfData.Get(L4::HashTablePerfCounter::CacheMissCount);
  stats.activeRecords = perfData.Get(L4::HashTablePerfCounter::RecordsCount);
  stats.evictedRecords = perfData.Get(L4::HashTablePerfCounter::EvictedRecordsCount);
  stats.evictedRecords = perfData.Get(L4::HashTablePerfCounter::EvictedRecordsCount);
  stats.totalSize = perfData.Get(L4::HashTablePerfCounter::TotalIndexSize) +
                    perfData.Get(L4::HashTablePerfCounter::TotalKeySize) +
                    perfData.Get(L4::HashTablePerfCounter::TotalValueSize);
  return stats;
}

ThreadUnsafeLRUCache::ThreadUnsafeLRUCache(const ThreadUnsafeLRUCache::Config &config)
    : storageSizeLimit_(config.sizeInMB * 1024 * 1024), storageSize_(0) {}

bool ThreadUnsafeLRUCache::fetch(const CacheKey &cacheKey, ProcessedRequestSentence &processedRequestSentence) {
  auto p = cache_.find(TranslationCache::hash(cacheKey));
  if (p != cache_.end()) {
    auto recordPtr = p->second;
    const Storage &value = recordPtr->value;
    string_view bytesView(value.data(), value.size());
    processedRequestSentence = std::move(ProcessedRequestSentence::fromBytesView(bytesView));

    // Refresh recently used
    auto record = std::move(*recordPtr);
    storage_.erase(recordPtr);
    storage_.insert(storage_.end(), std::move(record));

    ++stats_.hits;
    return true;
  }

  ++stats_.misses;
  return false;
}

void ThreadUnsafeLRUCache::insert(const CacheKey &cacheKey, const ProcessedRequestSentence &processedRequestSentence) {
  Record candidate;
  candidate.key = TranslationCache::hash(cacheKey);

  // Unfortunately we have to keep ownership within cache (with a clone). The original is sometimes owned by the items
  // that is forwarded for building response, std::move(...)  would invalidate this one.
  candidate.value = std::move(processedRequestSentence.cloneStorage());

  // Loop until not end and we have not found space to insert candidate adhering to configured storage limits.
  auto removeCandidatePtr = storage_.begin();
  while (storageSize_ + candidate.size() > storageSizeLimit_ && removeCandidatePtr != storage_.end()) {
    storageSize_ -= removeCandidatePtr->size();

    // Update cache stats
    ++stats_.evictedRecords;
    --stats_.activeRecords;
    stats_.totalSize = storageSize_;

    storage_.erase(removeCandidatePtr);
    ++removeCandidatePtr;
  }

  // Only insert new candidate if we have space after adhering to storage-limit
  if (storageSize_ + candidate.size() <= storageSizeLimit_) {
    auto recordPtr = storage_.insert(storage_.end(), std::move(candidate));
    cache_.emplace(std::make_pair(candidate.key, recordPtr));
    storageSize_ += candidate.size();

    // Update cache stats
    ++stats_.activeRecords;
    stats_.totalSize = storageSize_;
  }
}

TranslationCache::Stats ThreadUnsafeLRUCache::stats() {
  TranslationCache::Stats stats = stats_;
  return stats;
}

}  // namespace bergamot
}  // namespace marian
