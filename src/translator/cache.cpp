#include "translator/cache.h"

#include <cstring>

namespace marian {
namespace bergamot {

namespace {

template <class T>
const char *copyInAndAdvance(const char *src, T *dest, size_t num = 1) {
  const void *vsrc = reinterpret_cast<const void *>(src);
  void *vdest = reinterpret_cast<void *>(dest);
  std::memcpy(vdest, vsrc, num * sizeof(T));
  return reinterpret_cast<const char *>(src + num * sizeof(T));
}

template <class T>
const char *copyInVectorAndAdvance(const char *src, std::vector<T> &v) {
  size_t sizePrefix{0};
  src = copyInAndAdvance<size_t>(src, &sizePrefix);
  v.resize(sizePrefix);
  src = copyInAndAdvance<T>(src, v.data(), sizePrefix);
  return src;
}

template <class T>
void write(std::ostream &out, const T *data, size_t num = 1) {
  const char *cdata = reinterpret_cast<const char *>(data);
  out.write(cdata, num * sizeof(T));
}

template <class T>
void writeVector(std::ostream &out, std::vector<T> v) {
  size_t size = v.size();
  write<size_t>(out, &size);
  write<T>(out, v.data(), v.size());
}

}  // namespace

ProcessedRequestSentence::ProcessedRequestSentence() : sentenceScore_{0} {}

/// Construct from History
ProcessedRequestSentence::ProcessedRequestSentence(const History &history) {
  IPtr<Hypothesis> hypothesis;
  Result result = history.top();
  std::tie(words_, hypothesis, sentenceScore_) = result;
  softAlignment_ = hypothesis->tracebackAlignment();
  wordScores_ = hypothesis->tracebackWordScores();
}

void ProcessedRequestSentence::debug() {
  std::cout << "Words: " << words_.size() << std::endl;
  for (auto &word : words_) {
    std::cout << word.toString() << " ";
  }
  std::cout << std::endl;
  std::cout << "Alignments size: " << softAlignment_.size() << std::endl;
  for (size_t i = 0; i < softAlignment_.size(); i++) {
    std::cout << "# alignment[" << i << "] = " << softAlignment_[i].size() << " ";
  }
  std::cout << std::endl;
}

std::string ProcessedRequestSentence::toBytes() const {
  std::ostringstream out(std::ostringstream::binary);
  size_t softAlignmentSize = softAlignment_.size();
  write<size_t>(out, &softAlignmentSize);
  write<float>(out, &sentenceScore_);
  writeVector<marian::Word>(out, words_);
  for (auto &a : softAlignment_) {
    writeVector<float>(out, a);
  }
  writeVector<float>(out, wordScores_);
  return out.str();
}

/// Construct from stream of bytes
ProcessedRequestSentence ProcessedRequestSentence::fromBytes(char const *data, size_t size) {
  ProcessedRequestSentence sentence;
  char const *p = data;
  size_t softAlignmentSize{0};
  p = copyInAndAdvance<size_t>(p, &softAlignmentSize);
  p = copyInAndAdvance<float>(p, &sentence.sentenceScore_);
  std::cout << "softAlignmentSize: " << softAlignmentSize << " sentenceScore: " << sentence.sentenceScore_ << std::endl;

  size_t sizePrefix{0};
  sentence.softAlignment_.resize(softAlignmentSize);

  p = copyInVectorAndAdvance<marian::Word>(p, sentence.words_);
  for (size_t i = 0; i < softAlignmentSize; i++) {
    p = copyInVectorAndAdvance<float>(p, sentence.softAlignment_[i]);
  }

  p = copyInVectorAndAdvance<float>(p, sentence.wordScores_);
  return sentence;
}

// "numActionQueues" indicates how many action containers there will be in
// order to increase the throughput of registering an action.
// "performActionsInParallelThreshold" indicates the threshold value above
// which the actions are performed in parallel.
// "maxNumThreadsToPerformActions" indicates how many threads will be used
// when performing an action in parallel.
LockLessClockCache::LockLessClockCache(const std::string &modelIdentifier, size_t sizeInBytes, size_t timeOutInSeconds,
                                       bool removeExpired /* = true */)
    : epochManagerConfig_(/*epochQueueSize=*/1000,
                          /*epochProcessingInterval=*/std::chrono::milliseconds(100),
                          /*numActionQueues=*/1),
      cacheConfig_{sizeInBytes, std::chrono::seconds(timeOutInSeconds), removeExpired},
      modelIdentifier_(modelIdentifier),
      service_(epochManagerConfig_),
      context_(service_.GetContext()) {
  // Once a context is retrieved, the operations such as
  // operator[] on the context and Get() are lock-free.
  hashTableIndex_ = service_.AddHashTable(
      L4::HashTableConfig(modelIdentifier_, L4::HashTableConfig::Setting{/*numBuckets=*/1000000}, cacheConfig_));
}

bool LockLessClockCache::fetch(const Key &key, Value &value) {
  auto &hashTable = context_[hashTableIndex_];
  ValueBytes valBytes;
  KeyBytes keyBytes = keyToBytes(key);
  const marian::Word *begin = reinterpret_cast<const marian::Word *>(keyBytes.m_data);
  const marian::Word *end = reinterpret_cast<const marian::Word *>(keyBytes.m_data + keyBytes.m_size);

  bool fetchSuccess = hashTable.Get(keyBytes, valBytes);

  if (fetchSuccess) {
    std::cout << "Cache found, entry size = " << valBytes.m_size << std::endl;
    for (auto p = begin; p != end; p++) {
      std::cout << p->toString() << " ";
    }
    std::cout << std::endl;
    value = ProcessedRequestSentence::fromBytes(reinterpret_cast<const char *>(valBytes.m_data), valBytes.m_size);
    value.debug();
  }

  return fetchSuccess;
}

void LockLessClockCache::insert(const Key &key, const Value &value) {
  auto &hashTable = context_[hashTableIndex_];
  KeyBytes keyBytes = keyToBytes(key);

  ValueBytes valBytes;
  std::string serialized = value.toBytes();
  ABORT_IF(serialized.size() == 0, "Refusing to store empty string");

  valBytes.m_data = reinterpret_cast<const std::uint8_t *>(serialized.data());
  valBytes.m_size = sizeof(std::string::value_type) * serialized.size();

  hashTable.Add(keyBytes, valBytes);
}

CacheStats LockLessClockCache::stats() const {
  auto &perfData = context_[hashTableIndex_].GetPerfData();
  CacheStats stats;
  stats.hits = perfData.Get(L4::HashTablePerfCounter::CacheHitCount);
  stats.misses = perfData.Get(L4::HashTablePerfCounter::CacheMissCount);
  return stats;
};

LockLessClockCache::KeyBytes LockLessClockCache::keyToBytes(const Key &key) {
  KeyBytes keyBytes;
  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(key.data());
  keyBytes.m_size = sizeof(Key::value_type) * key.size();
  return keyBytes;
}

}  // namespace bergamot
}  // namespace marian
