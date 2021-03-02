/// It's best not to look at the horrors in this file at this stage.
///
/// The idea is to create an Annotation (SentenceRangesT) which can be used
/// using std::string_view units and absl::string_view units.
/// Objective is to be able to pluck the marian::bergamot::* for decoder
/// replacement (maintaining C++11) while allowing bergamot-translator to be in
/// C++17.
///
/// Annotation
//  = SentenceRangesT<std::string_view> or SentenceRangesT<absl::string_view>.
///
/// With this in place and a consistent API (access through SentenceRangesT), we
/// might be able to replace string_view with indices (size_t begin, size_t end)
/// instead of (const char*, size_t size). This would enable defining
/// copy-construction without invalidating the annotations.
///
/// In a next step, we see source and translation as AnnotatedBlobs. This is
/// used both in marian::bergamot::Response and TranslationResult.
///
/// AnnotatedBlob = Annotation + std::string blob which the annotation refers
/// to.

#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include "data/types.h"
#include <cassert>
#include <vector>

namespace marian {
namespace bergamot {

/// SentenceRanges stores string_view_types into a source text, with additional
/// annotations to mark sentence boundaries.
///
/// Given the availability annotations, this container provides capabilty to
/// add sentences, and access individual sentences.
template <class string_view_type> class SentenceRangesT {
public:
  typedef typename std::vector<string_view_type>::iterator WordIterator;

  // Adds a sentence, used to load from SentencePiece annotations conveniently.
  // encodeWithByteRanges and decodeWithByteRanges.
  void addSentence(std::vector<string_view_type> &wordRanges) {
    addSentence(std::begin(wordRanges), std::end(wordRanges));
  };

  void addSentence(WordIterator begin, WordIterator end) {
    size_t size = flatByteRanges_.size();
    flatByteRanges_.insert(std::end(flatByteRanges_), begin, end);
    sentenceBeginIds_.push_back(size);
  };

  void clear() {
    flatByteRanges_.clear();
    sentenceBeginIds_.clear();
  }

  // Returns the number of sentences annotated in a blob.
  size_t numSentences() const { return sentenceBeginIds_.size(); }

  // Returns a string_view_type into the ith sentence.
  string_view_type sentence(size_t index) const {
    size_t bosId;
    string_view_type eos, bos;

    bosId = sentenceBeginIds_[index];
    bos = flatByteRanges_[bosId];

    if (index + 1 == numSentences()) {
      eos = flatByteRanges_.back();
    } else {
      assert(index < numSentences());
      size_t eosId = sentenceBeginIds_[index + 1];
      --eosId;
      eos = flatByteRanges_[eosId];
    }

    return sentenceBetween(bos, eos);
  };

  // Provides access to the internal word, for the i-th sentencs.
  string_view_type word(size_t sentenceIdx, size_t wordIdx) const {
    size_t offset = sentenceBeginIds_[sentenceIdx];
    assert(offset + wordIdx < flatByteRanges_.size());
    return flatByteRanges_[offset + wordIdx];
  }

  /// Allow emty construction, useful when I have translation and annotations
  /// not initialized but to be initialized later from histories_.
  SentenceRangesT() {}

  /// Allows conversion from one string_view type to another. For our case
  /// converting from marian::bergamot::Response (absl::string_view) ->
  /// TranslationResult (std::string_view)
  //
  template <class src_type>
  SentenceRangesT(const SentenceRangesT<src_type> &other) {
    sentenceBeginIds_ = other.sentenceBeginIds();
    for (auto &sview : other.flatByteRanges()) {
      flatByteRanges_.emplace_back(sview.data(), sview.size());
    }
  };

  // Const reference required for use in the conversion function.
  const std::vector<string_view_type> &flatByteRanges() const {
    return flatByteRanges_;
  }

  // Const reference required for use in the conversion function.
  const std::vector<size_t> &sentenceBeginIds() const {
    return sentenceBeginIds_;
  }

private:
  // A flat storage for string_view_types. Can be words or sentences.
  std::vector<string_view_type> flatByteRanges_;

  // The container grows dynamically with addSentence. size_t marking index is
  // used to ensure the sentence boundaries stay same while underlying storage
  // might be changed during reallocation.
  std::vector<size_t> sentenceBeginIds_;

  // Utility function to extract the string starting at firstWord and ending at
  // lastWord as a single string-view.
  string_view_type sentenceBetween(string_view_type firstWord,
                                   string_view_type lastWord) const {

    const char *data = firstWord.data();
    size_t size = lastWord.data() + lastWord.size() - firstWord.data();
    return string_view_type(data, size);
  };
};

// This typedef allows a non-invasive modification (as SentenceRanges are
// present in many files which operate during the lifecycle of a Request).
typedef SentenceRangesT<string_view> SentenceRanges;

/// An AnnotatedBlob is blob std::string along with the annotation which are
/// valid until the original blob is valid (string shouldn't be invalidated).
/// Annotated Blob, due to its nature binding a blob to string_view should only
/// be move-constructible as a whole.

template <class Annotation> struct AnnotatedBlobT {
  std::string blob;
  SentenceRangesT<Annotation> annotation;
  AnnotatedBlobT(){};

  // Annotated Blobs can also be converted between string_views. This templated
  // constructor allows for the same.
  template <class src_annotation_type>
  AnnotatedBlobT(std::string &&blob,
                 SentenceRangesT<src_annotation_type> &&annotation)
      : blob(std::move(blob)), annotation(annotation) {}

  AnnotatedBlobT(std::string &&blob, SentenceRangesT<Annotation> &&annotation)
      : blob(std::move(blob)), annotation(std::move(annotation)) {}

  template <class src_annotation_type>
  AnnotatedBlobT(AnnotatedBlobT<src_annotation_type> &&other)
      : blob(std::move(other.blob)), annotation(std::move(other.annotation)) {}

  /// AnnotatedBlobs are disallowed to copy-construct / copy-assign since the
  /// string_views are invalidated during copy-op.
  AnnotatedBlobT(const AnnotatedBlobT &other) = delete;
  AnnotatedBlobT &operator=(const AnnotatedBlobT &other) = delete;
  const size_t numSentences() const { return annotation.numSentences(); }
};

} // namespace bergamot

} // namespace marian

#endif //  BERGAMOT_SENTENCE_RANGES_H_
