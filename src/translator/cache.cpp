#include "translator/cache.h"

#include <cstring>

namespace marian {
namespace bergamot {

namespace {

// Generic write/read from pointers

// write uses ostream, to get bytes/blob use with ostringstream(::binary)
template <class T>
void write(std::ostream &out, const T *data, size_t num = 1) {
  const char *cdata = reinterpret_cast<const char *>(data);
  out.write(cdata, num * sizeof(T));
}

// L4 stores the data as blobs. These use memcpy to construct parts from the blob given the start and size. It is the
// responsibility of the caller to prepare the container with the correct contiguous size so memcpy works correctly.
template <class T>
const char *copyInAndAdvance(const char *src, T *dest, size_t num = 1) {
  const void *vsrc = reinterpret_cast<const void *>(src);
  void *vdest = reinterpret_cast<void *>(dest);
  std::memcpy(vdest, vsrc, num * sizeof(T));
  return reinterpret_cast<const char *>(src + num * sizeof(T));
}

// Specializations to read and write vectors. The format stored is [size, v_0, v_1 ... v_size]
template <class T>
void writeVector(std::ostream &out, std::vector<T> v) {
  size_t size = v.size();
  write<size_t>(out, &size);
  write<T>(out, v.data(), v.size());
}

template <class T>
const char *copyInVectorAndAdvance(const char *src, std::vector<T> &v) {
  // Read in size of the vector
  size_t sizePrefix{0};
  src = copyInAndAdvance<size_t>(src, &sizePrefix);

  // Ensure contiguous memory location exists for memcpy inside copyInAndAdvance
  v.reserve(sizePrefix);

  // Read in the vector
  src = copyInAndAdvance<T>(src, v.data(), sizePrefix);
  return src;
}

}  // namespace

ProcessedRequestSentence::ProcessedRequestSentence() : sentenceScore_{0} {}

/// Construct from History
ProcessedRequestSentence::ProcessedRequestSentence(const History &history) {
  // Convert marian's lazy shallow-history, consolidating just the information we want.
  IPtr<Hypothesis> hypothesis;
  Result result = history.top();
  std::tie(words_, hypothesis, sentenceScore_) = result;
  softAlignment_ = hypothesis->tracebackAlignment();
  wordScores_ = hypothesis->tracebackWordScores();
}

std::string ProcessedRequestSentence::toBytes() const {
  // Note: write in order of member definitions in class.
  std::ostringstream out(std::ostringstream::binary);
  writeVector<marian::Word>(out, words_);

  size_t softAlignmentSize = softAlignment_.size();
  write<size_t>(out, &softAlignmentSize);
  for (auto &alignment : softAlignment_) {
    writeVector<float>(out, alignment);
  }

  write<float>(out, &sentenceScore_);
  writeVector<float>(out, wordScores_);
  return out.str();
}

/// Construct from stream of bytes
ProcessedRequestSentence ProcessedRequestSentence::fromBytes(char const *data, size_t size) {
  ProcessedRequestSentence sentence;
  char const *p = data;

  p = copyInVectorAndAdvance<marian::Word>(p, sentence.words_);

  size_t softAlignmentSize{0};
  p = copyInAndAdvance<size_t>(p, &softAlignmentSize);
  sentence.softAlignment_.resize(softAlignmentSize);

  for (size_t i = 0; i < softAlignmentSize; i++) {
    p = copyInVectorAndAdvance<float>(p, sentence.softAlignment_[i]);
  }

  p = copyInAndAdvance<float>(p, &sentence.sentenceScore_);
  p = copyInVectorAndAdvance<float>(p, sentence.wordScores_);
  return sentence;
}

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
  // Once a context is retrieved, the operations such as
  // operator[] on the context and Get() are lock-free.
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
    /*
  std::cout << "--- L4: " << label << std::endl;
  auto &perfData = context_[hashTableIndex_].GetPerfData();
#define __l4inspect(key) std::cout << #key << " " << perfData.Get(L4::HashTablePerfCounter::key) << std::endl;

  __l4inspect(CacheHitCount);
  __l4inspect(CacheMissCount);
  __l4inspect(RecordsCount);
  __l4inspect(EvictedRecordsCount);
  __l4inspect(TotalIndexSize);
  __l4inspect(TotalKeySize);
  __l4inspect(TotalValueSize);

  std::cout << "---- " << std::endl;
  */
};

ThreadUnsafeCache::ThreadUnsafeCache(size_t sizeInBytes, size_t timeOutInSeconds, bool removeExpired)
    : storageSizeLimit_(sizeInBytes), storageSize_(0) {}

bool ThreadUnsafeCache::fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence) {
  auto p = cache_.find(words);
  if (p != cache_.end()) {
    processedRequestSentence = p->second;
  }
  return false;
}

void ThreadUnsafeCache::insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence) {
  // TODO(measure storage size, evict randomly, or maybe use clock
  cache_.emplace(words, processedRequestSentence);
}

}  // namespace bergamot
}  // namespace marian
