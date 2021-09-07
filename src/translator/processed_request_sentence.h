#ifndef SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_
#define SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_

#include <memory>
#include <string>
#include <vector>

#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

/// Motivation: Our cache requires flat-storage for L4 to operate. Towards this, we bake in a flat, sequential binary
/// storage which is written onto (copied) from objects we require later on from marian::History
///
/// This class operates with a pointer to the start, templated by type - with which we can provide a "view" onto the
/// flat sequential binary backend, for a convenient API for the rest of the code resembling the true underlying type.
///
/// This class provides read-only APIs and the following convention is baked into code for the layout. A Range is
/// stored as:
///     [n, v0, v1, ... v_{n-1}]
/// where n = number of elements, and v_{i} is the i-th element.

template <class T>
class ConstRangeView {
 public:
  using value_type = T;
  ConstRangeView() : sizeLoc_(nullptr), begin_{nullptr}, end_{nullptr} {}

  /// Builds a ConstRangeView provided a pointer and type.
  explicit ConstRangeView(const void *ptr) {
    sizeLoc_ = reinterpret_cast<const std::size_t *>(ptr);

    // Advance length of one std::size_t to get vector begin
    begin_ = reinterpret_cast<const T *>(reinterpret_cast<const char *>(ptr) + sizeof(std::size_t));

    // End is simply begin + number of elements now
    end_ = begin_ + size();
  }

  size_t memSize() const {
    return (                  //
        sizeof(std::size_t)   // size_
        + sizeof(T) * size()  // container elements
    );
  }

  const T *cbegin() const { return begin_; }
  const T *cend() const { return end_; }

  size_t size() const {
    assert(sizeLoc_ != nullptr);
    return *sizeLoc_;
  }

  static size_t storageSize(const std::vector<T> &v) {
    return (                    //  Binary math follows
        sizeof(std::size_t)     // The size variable written at the start.
        + sizeof(T) * v.size()  // Vector elements.
    );
  }

 private:
  const T *begin_;
  const T *end_;
  const size_t *sizeLoc_;
};

/// Underlying flat sequential binary storage for entities in a ProcessedRequestSentence. Views are provided onto this
/// storage to mimick ranges. This class allows for optimizing storage for L4-caches continuous binary representation
/// requirements.
class Storage {
  friend class StorageIO;

 public:
  /// To be used in conjunction with delayedAllocate. A ProcessedRequestSentence is created uninitialized. Upon incoming
  /// History, we compute the requireSize and allocate later with delayed allocate.
  Storage() : data_{nullptr}, size_(0) {}

  void delayedAllocate(size_t requiredSize) {
    assert(!initialized());
    size_ = requiredSize;
    data_ = malloc(sizeof(char) * size_);
  }

  /// Copy contents of size bytes from data. To be used to copy from cache holding.
  Storage(const void *data, size_t size) : size_(size) {
    data_ = malloc(sizeof(char) * size);
    std::memcpy(data_, data, size);
  }

  ~Storage() {
    if (initialized()) {
      free(data_);
    }
  }

  /// Forbid copy assignment for performance optimizations.
  Storage(const Storage &storage) = delete;

  /// Forbid copy assignment for performance optimizations.
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

  inline bool initialized() const { return data_ != nullptr; }

  const void *data() const { return data_; }
  size_t size() const { return size_; }

 private:
  void *data_;
  size_t size_;
};

class StorageIO {
 public:
  // Helper methods to load "view" types backed by storage.
  StorageIO(Storage &storage) : storage_(storage) { ptr_ = storage_.data_; }

  template <class T>
  const T *readVar() {
    T *var = reinterpret_cast<T *>(ptr_);
    ptr_ = advance(sizeof(T));
    return var;
  }

  template <class T>
  const T *writeVar(const T &value) {
    // Variable is marked to point to pointer, then the incoming value written.
    T *var = reinterpret_cast<T *>(ptr_);
    *var = value;

    // Advance pointer after write.
    ptr_ = advance(sizeof(T));
    return var;
  }

  template <class T>
  ConstRangeView<T> readRange() {
    ConstRangeView<T> view(ptr_);
    ptr_ = advance(view.memSize());
    return view;
  }

  template <class T>
  ConstRangeView<T> writeRange(const std::vector<T> &v) {
    // Modification of pointer happens only here.
    *(reinterpret_cast<size_t *>(ptr_)) = v.size();
    T *begin = reinterpret_cast<T *>(reinterpret_cast<char *>(ptr_) + sizeof(std::size_t));
    std::copy(v.begin(), v.end(), begin);

    // Prepare view.
    ConstRangeView<T> view(ptr_);
    ptr_ = advance(view.memSize());

    return view;
  }

 private:
  inline void *advance(size_t bytes) {
    void *ptrAfter = reinterpret_cast<void *>(reinterpret_cast<char *>(ptr_) + bytes);
    const void *end = reinterpret_cast<const void *>(reinterpret_cast<const char *>(storage_.data()) + storage_.size());
    assert(ptrAfter <= end);
    return ptrAfter;
  }

  void *ptr_;
  Storage &storage_;
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

  /// Deserialize the contents of an instance from a sequence of bytes. Compatible with `toBytesView`.
  static ProcessedRequestSentence fromBytesView(const string_view &bytesView);

  /// Serialize the contents of an instance to a sequence of bytes. Compatible with `fromBytesView`.
  string_view toBytesView() const;

  /// Returns if a ProcessedRequestSentence is empty.
  bool empty() const { return !storage_.initialized(); }

  // Const accessors for private members
  const Words &words() const { return words_; }
  const SoftAlignment &softAlignment() const { return softAlignment_; }
  const WordScores &wordScores() const { return wordScores_; }
  float sentenceScore() const { return *sentenceScorePtr_; }

  Storage cloneStorage() const { return Storage(storage_.data(), storage_.size()); }

 private:
  ProcessedRequestSentence(const string_view &bytesview);

  // All types are aliased within this class.
  Words words_;

  // Nested variable length lists are tricky, for now we go for easy implementation.
  SoftAlignment softAlignment_;
  const size_t *softAlignmentSizePtr_;

  WordScores wordScores_;
  const float *sentenceScorePtr_;

  Storage storage_;
};

typedef std::vector<ProcessedRequestSentence> ProcessedRequestSentences;

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_PROCESSED_REQUEST_SENTENCE_H_
