#include "translator/cache.h"

namespace marian {
namespace bergamot {

namespace cache_util {

std::string wordsToString(const marian::Words &words) {
  std::string repr;
  for (size_t i = 0; i < words.size(); i++) {
    if (i != 0) {
      repr += " ";
    }
    repr += words[i].toString();
  }
  return repr;
}

}  // namespace cache_util

#ifndef WASM_COMPATIBLE_SOURCE
LockLessClockCache::LockLessClockCache(size_t sizeInBytes, size_t timeOutInSeconds, bool removeExpired /* = true */)
    : epochManagerConfig_(/*epochQueueSize=*/1000,
                          /*epochProcessingInterval=*/std::chrono::milliseconds(1000),
                          /*numActionQueues=*/4),
      cacheConfig_{sizeInBytes, std::chrono::seconds(timeOutInSeconds), removeExpired},
      service_(epochManagerConfig_),
      context_(service_.GetContext()) {
  // For now, we use a hard-coded modelIdentifier; In the future we may keep separate caches for models or change the
  // Key to use <modelIdentifier, marian::Words> to keep a global lid on cache memory usage.
  //
  // L4's API requires this string identifier, which for now is called "global-cache"

  const std::string modelIdentifier = "global-cache";
  hashTableIndex_ = service_.AddHashTable(
      L4::HashTableConfig(modelIdentifier, L4::HashTableConfig::Setting{/*numBuckets=*/10000}, cacheConfig_));
}

bool LockLessClockCache::fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence) {
  auto &hashTable = context_[hashTableIndex_];

  KeyBytes keyBytes;
  std::string keyStr = cache_util::wordsToString(words);

  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(keyStr.data());
  keyBytes.m_size = sizeof(std::string::value_type) * keyStr.size();

  ValueBytes valBytes;
  bool fetchSuccess = hashTable.Get(keyBytes, valBytes);
  if (fetchSuccess) {
    processedRequestSentence =
        ProcessedRequestSentence::fromBytes(reinterpret_cast<const char *>(valBytes.m_data), valBytes.m_size);
  }

  debug("After Fetch");
  return fetchSuccess;
}

void LockLessClockCache::insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence) {
  auto context = service_.GetContext();
  auto &hashTable = context[hashTableIndex_];

  KeyBytes keyBytes;
  std::string keyStr = cache_util::wordsToString(words);

  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(keyStr.data());
  keyBytes.m_size = sizeof(std::string::value_type) * keyStr.size();

  ValueBytes valBytes;
  std::string serialized = processedRequestSentence.toBytes();

  valBytes.m_data = reinterpret_cast<const std::uint8_t *>(serialized.data());
  valBytes.m_size = sizeof(std::string::value_type) * serialized.size();

  debug("Before Add");
  hashTable.Add(keyBytes, valBytes);
  debug("After Add");
}

CacheStats LockLessClockCache::stats() const {
  auto &perfData = context_[hashTableIndex_].GetPerfData();
  CacheStats stats;
  stats.hits = perfData.Get(L4::HashTablePerfCounter::CacheHitCount);
  stats.misses = perfData.Get(L4::HashTablePerfCounter::CacheMissCount);
  return stats;
}

void LockLessClockCache::debug(std::string label) const {
  std::cerr << "--- L4: " << label << std::endl;
  auto &perfData = context_[hashTableIndex_].GetPerfData();
#define __l4inspect(key) std::cout << #key << " " << perfData.Get(L4::HashTablePerfCounter::key) << std::endl;

  __l4inspect(CacheHitCount);
  __l4inspect(CacheMissCount);
  __l4inspect(RecordsCount);
  __l4inspect(EvictedRecordsCount);
  __l4inspect(TotalIndexSize);
  __l4inspect(TotalKeySize);
  __l4inspect(TotalValueSize);

  std::cerr << "---- " << std::endl;

#undef __l4inspect
};

#endif

ThreadUnsafeLRUCache::ThreadUnsafeLRUCache(size_t sizeInBytes, size_t timeOutInSeconds, bool removeExpired)
    : storageSizeLimit_(sizeInBytes), storageSize_(0) {}

bool ThreadUnsafeLRUCache::fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence) {
  auto p = cache_.find(words);
  if (p != cache_.end()) {
    auto recordPtr = p->second;
    const std::string &value = recordPtr->value;
    processedRequestSentence = ProcessedRequestSentence::fromBytes(value.data(), value.size());

    // Refresh recently used
    storageSize_ -= recordPtr->size();
    auto record = *recordPtr;
    storage_.erase(recordPtr);
    storage_.insert(storage_.end(), std::move(record));

    ++stats_.hits;
    return true;
  }

  ++stats_.misses;
  return false;
}

void ThreadUnsafeLRUCache::insert(const marian::Words &words,
                                  const ProcessedRequestSentence &processedRequestSentence) {
  Record candidate{words, processedRequestSentence.toBytes()};
  auto removeCandidatePtr = storage_.begin();

  while (storageSize_ + candidate.size() > storageSizeLimit_ && removeCandidatePtr != storage_.end()) {
    storageSize_ -= removeCandidatePtr->size();
    storage_.erase(removeCandidatePtr);
    ++removeCandidatePtr;
  }

  if (storageSize_ + candidate.size() <= storageSizeLimit_) {
    auto recordPtr = storage_.insert(storage_.end(), std::move(candidate));
    cache_.emplace(std::make_pair(words, recordPtr));
    storageSize_ += candidate.size();
  }
}

CacheStats ThreadUnsafeLRUCache::stats() const {
  CacheStats stats = stats_;
  return stats;
}

}  // namespace bergamot
}  // namespace marian
