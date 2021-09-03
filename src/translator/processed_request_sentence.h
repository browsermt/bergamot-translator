#ifndef SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_
#define SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_

#include <memory>
#include <string>
#include <vector>

#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

// Underlying storage
class Storage {
 public:
  Storage() : data_{nullptr}, size_(0) {}

  void delayedAllocate(size_t requiredSize) {
    assert(data_ == nullptr);
    size_ = requiredSize;
    data_ = malloc(sizeof(char) * size_);
  }

  Storage(const void *data, size_t size) : size_(size) {
    data_ = malloc(sizeof(char) * size);
    std::memcpy(data_, data, size);
  }

  ~Storage() { free(data_); }

  Storage(const Storage &storage) = delete;
  Storage &operator=(const Storage &storage) = delete;

  Storage(Storage &&other) : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
  }

  Storage &operator=(Storage &&other) {
    data_ = other.data_;
    size_ = other.size_;

    // Set the other memory to uninitialized
    other.data_ = nullptr;
    other.size_ = 0;
    return *this;
  }

  bool initialized() const { return data_ != nullptr; }

  void *data() const { return data_; }
  size_t size() const { return size_; }

 private:
  void *data_;
  size_t size_;
};

/// Motivation: Our cache requires flat-storage for L4 to operate. Towards this, we bake in a flat, sequential binary
/// storage which is written onto (copied) from objects we require later on from marian::History
///
/// This class operates with a pointer to the start, templated by type - with which we can provide a "view" onto the
/// flat sequential binary backend, for a convenient API for the rest of the code resembling the true underlying type.
///
/// This class provides read-write APIs and the following convention is baked into code for the layout. A Range is
/// stored as:
///     [n, v0, v1, ... v_{n-1}]
/// where n = number of elements, and v_{i} is the i-th element.

template <class T>
class ConstRangeView {
 public:
  ConstRangeView() : size_(0), begin_{nullptr}, end_{nullptr} {}

  /// Builds a ConstRangeView provided a pointer and type.
  explicit ConstRangeView(const void *ptr) {
    size_ = *(reinterpret_cast<const std::size_t *>(ptr));

    // Advance length of one std::size_t to get vector begin
    begin_ = reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) + sizeof(std::size_t));

    // End is simply begin + number of elements now
    end_ = begin_ + size_;
  }

  /// Writes starting at *ptr the vector<T> v in the layout expected by this class.
  /// Increments ptr internally to ensure no overwrites.
  ConstRangeView(const std::vector<T> &v, void *&ptr) {
    size_ = v.size();

    // Setting up pointers for writes.
    size_t *sizeLoc = reinterpret_cast<std::size_t *>(ptr);
    begin_ = reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) + sizeof(std::size_t));
    end_ = begin_ + size_;

    // Writes happens below.
    *sizeLoc = v.size();
    std::copy(v.begin(), v.end(), begin_);

    // Update ptr so that overwrites don't happen.
    ptr = reinterpret_cast<void *>(reinterpret_cast<char *>(ptr) + storageSize(v));
  }

  const T *begin() const { return begin_; }
  const T *end() const { return end_; }
  size_t size() const { return size_; }

  static size_t storageSize(const std::vector<T> &v) {
    return (                    //  Binary math follows
        sizeof(std::size_t)     // The size variable written at the start.
        + sizeof(T) * v.size()  // Vector elements.
    );
  }

 private:
  T *begin_;
  T *end_;
  size_t size_;
};

/// History is marian object with plenty of `shared_ptr`s lying around, which makes keeping a lid on memory hard for
/// cache, due to copies being shallow rather than deep. We therefore process marian's lazy History management changing
/// to something eager, keeping only what we want and units accounted for. Each of these corresponds to a
/// `RequestSentence` and retains the minimum information required to reconstruct a full `Response` if cached.
///
/// L4 requires byte-streams, which allows for it to keep tight control over "memory" we need to have accounting for
/// cache-size based on memory/bytes as well for any arbitrary serializable into bytestream data-types.
///
/// An empty() method is provided which relays if no words are present. This empty type is also treated as  an
/// incomplete ProcessedRequestSentence during cache operations.
class ProcessedRequestSentence {
 public:
  using Words = ConstRangeView<marian::Word>;
  using DistSourceGivenTarget = ConstRangeView<float>;
  using SoftAlignment = std::vector<DistSourceGivenTarget>;
  using WordScores = ConstRangeView<float>;

  /// Constructs an empty (uninitialized) ProcessedRequestSentence.
  ProcessedRequestSentence();

  /// Construct from History - consolidate members which we require further and store the complete object (not
  /// references to shared-pointers) for storage and recompute efficiency purposes.
  ProcessedRequestSentence(const History &history);

  ProcessedRequestSentence(ProcessedRequestSentence &&) = default;
  ProcessedRequestSentence &operator=(ProcessedRequestSentence &&) = default;
  // The following: fromBytes(...) and toBytes(...) are helpers to work with blobs / bytearray storing the class,
  // particularly for L4. Storage follows the order of member definitions in this class. With vectors prefixed with
  // sizes to allocate before reading in with the right sizes.

  /// Deserialize the contents of an instance from a sequence of bytes. Compatible with `toBytes`.
  static ProcessedRequestSentence fromBytes(const char *data, size_t size);

  /// Serialize the contents of an instance to a sequence of bytes. Compatible with `fromBytes`.
  string_view byteStorage() const;

  /// Returns if a ProcessedRequestSentence is empty.
  bool empty() const { return !storage_.initialized(); }

  /// Prints L4 debug information during cache ops if environment variable `BERGAMOT_L4_DEBUG_CACHE` is set. This is a
  /// development artifact and maybe removed in the future.
  void debug();

  // Const accessors for private members
  const Words &words() const { return words_; }
  const SoftAlignment &softAlignment() const { return softAlignment_; }
  const WordScores &wordScores() const { return wordScores_; }
  float sentenceScore() const { return *sentenceScorePtr_; }

 private:
  ProcessedRequestSentence(const char *data, size_t size);

  // All types are aliased within this class.
  Words words_;

  // Nested variable lists are tricky, for now we go for easy implementation.
  SoftAlignment softAlignment_;
  size_t *softAlignmentSizePtr_;

  WordScores wordScores_;
  float *sentenceScorePtr_;

  Storage storage_;
};

typedef std::vector<ProcessedRequestSentence> ProcessedRequestSentences;

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_
