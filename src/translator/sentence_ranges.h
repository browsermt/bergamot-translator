/// It's best not to look at the horrors in this file at this stage.
///
/// The idea is to create an Annotation (AnnotationT) which can be used
/// using std::string_view units and absl::string_view units.
/// Objective is to be able to pluck the marian::bergamot::* for decoder
/// replacement (maintaining C++11) while allowing bergamot-translator to be in
/// C++17.
///
/// Annotation
//  = AnnotationT<std::string_view> or AnnotationT<absl::string_view>.
///
/// With this in place and a consistent API (access through AnnotationT), we
/// might be able to replace string_view with indices (size_t begin, size_t end)
/// instead of (const char*, size_t size). This would enable defining
/// copy-construction without invalidating the annotations.
///
/// In a next step, we see source and translation as AnnotatedBlobs. This is
/// used both in marian::bergamot::Response and TranslationResult.
///
/// AnnotatedBlob = Annotation + std::string blob (which the annotation refers
/// to.)

#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include "data/types.h"
#include <cassert>
#include <utility>
#include <vector>

namespace marian {
namespace bergamot {

/// SentenceRanges stores string_view_types into a source text, with additional
/// annotations to mark sentence boundaries.
///
/// Given the availability annotations, this container provides capabilty to
/// add sentences, and access individual sentences.
template <class string_view_type> class AnnotationT {
public:
  typedef typename std::vector<string_view_type>::iterator WordIterator;

  /// Allow emty construction, useful when I have translation and annotations
  /// not initialized but to be initialized later from histories_.
  AnnotationT() {}

  /// Allows conversion from one string_view type to another. For our case
  /// converting from marian::bergamot::Response (absl::string_view) ->
  /// TranslationResult (std::string_view)
  template <class other_annotation_type>
  AnnotationT(const AnnotationT<other_annotation_type> &other) {
    sentenceBeginIds_ = other.sentenceBeginIds();
    for (auto &sview : other.flatByteRanges()) {
      flatByteRanges_.emplace_back(sview.data(), sview.size());
    }
  };

  /// Adds a sentence, used to load from SentencePiece annotations conveniently.
  /// encodeWithByteRanges and decodeWithByteRanges.
  void addSentence(std::vector<string_view_type> &wordRanges) {
    addSentence(std::begin(wordRanges), std::end(wordRanges));
  };

  /// Adds a sentence between two iterators, often useful while constructing
  /// from parts of a container.
  void addSentence(WordIterator begin, WordIterator end) {
    size_t size = flatByteRanges_.size();
    flatByteRanges_.insert(std::end(flatByteRanges_), begin, end);
    sentenceBeginIds_.push_back(size);
  };

  /// clears internal containers, finds use in at least copy-assignment.
  void clear() {
    flatByteRanges_.clear();
    sentenceBeginIds_.clear();
  }

  /// Returns the number of sentences annotated in a blob.
  size_t numSentences() const { return sentenceBeginIds_.size(); }

  /// Returns indices of terminal (word) string_views of a sentence, in
  /// flatByteRanges_.
  std::pair<size_t, size_t> sentenceTerminals(size_t sentenceIdx) const {
    size_t bosId, eosId;
    bosId = sentenceBeginIds_[sentenceIdx];
    eosId = sentenceIdx + 1 < numSentences()
                ? sentenceBeginIds_[sentenceIdx + 1] - 1
                : flatByteRanges_.size() - 1;
    return std::make_pair(bosId, eosId);
  }

  /// Returns a string_view_type into the sentence corresponding to sentenceIdx
  string_view_type sentence(size_t sentenceIdx) const {
    auto terminals = sentenceTerminals(sentenceIdx);
    string_view_type eos, bos;
    bos = flatByteRanges_[terminals.first];
    eos = flatByteRanges_[terminals.second];
    return sentenceBetween(bos, eos);
  };

  /// Provides access to the internal word correspoding to wordIdx, for the
  /// sentence corresponding to sentenceIdx.
  string_view_type word(size_t sentenceIdx, size_t wordIdx) const {
    size_t offset = sentenceBeginIds_[sentenceIdx];
    assert(offset + wordIdx < flatByteRanges_.size());
    return flatByteRanges_[offset + wordIdx];
  }

  /// Allows access to a string_view using flatIdx
  string_view_type wordFromFlatIdx(size_t flatIdx) const {
    return flatByteRanges_[flatIdx];
  }

  /// Const reference required for use in the conversion function.
  const std::vector<string_view_type> &flatByteRanges() const {
    return flatByteRanges_;
  }

  /// Const reference required for use in the conversion function.
  const std::vector<size_t> &sentenceBeginIds() const {
    return sentenceBeginIds_;
  }

private:
  /// A flat storage for string_view_types. Can be words or sentences.
  std::vector<string_view_type> flatByteRanges_;

  /// The container grows dynamically with addSentence. size_t marking index is
  /// used to ensure the sentence boundaries stay same while underlying storage
  /// might be changed during reallocation.
  std::vector<size_t> sentenceBeginIds_;

  /// Utility function to extract the string starting at firstWord and ending at
  /// lastWord as a single string-view.
  string_view_type sentenceBetween(string_view_type firstWord,
                                   string_view_type lastWord) const {

    const char *data = firstWord.data();
    size_t size = lastWord.data() + lastWord.size() - firstWord.data();
    return string_view_type(data, size);
  };
};

// This typedef allows a non-invasive modification (as SentenceRanges are
// present in many files which operate during the lifecycle of a Request).
typedef AnnotationT<string_view> SentenceRanges;

/// An AnnotatedBlob is blob std::string along with the annotation which are
/// valid until the original blob is valid (string shouldn't be invalidated).
/// Annotated Blob, due to its nature binding a blob to string_view should only
/// be move-constructible as a whole.

template <class annotation_type> struct AnnotatedBlobT {
public:
  std::string blob;
  AnnotationT<annotation_type> annotation;

  AnnotatedBlobT(){};

  /// Proper constructor.
  AnnotatedBlobT(std::string &&blob, AnnotationT<annotation_type> &&annotation)
      : blob(std::move(blob)), annotation(std::move(annotation)) {}

  /// An improper constructor. Annotated Blobs "can" also be converted
  /// between string_views. This templated constructor allows for the same.
  template <class other_annotation_type>
  AnnotatedBlobT(std::string &&blob,
                 AnnotationT<other_annotation_type> &&annotation)
      : blob(std::move(blob)), // string is moved.
        annotation(annotation) // slightly-sneaky move. string_views are
                               // converted between types.
  {}

  //  TODO(jerinphilip): verify why I need other_annotation_type here. Does not
  //  compile without the template.
  /// Move-constructor.
  template <class other_annotation_type>
  AnnotatedBlobT(AnnotatedBlobT<other_annotation_type> &&other)
      : blob(std::move(other.blob)), annotation(std::move(other.annotation)) {}

  /// Returns the number of sentences in the annotation structure.
  const size_t numSentences() const { return annotation.numSentences(); }

  /// CopyConstructor. See loadFrom.
  template <class other_annotation_type>
  AnnotatedBlobT(const AnnotatedBlobT<other_annotation_type> &other) {
    loadFrom(other);
  }

  /// CopyAssignment overload. See loadFrom.
  template <class other_annotation_type>
  AnnotatedBlobT &
  operator=(const AnnotatedBlobT<other_annotation_type> &other) {
    loadFrom(other);
    return *this;
  }

private:
  /// loadFrom is reused for CopyConstruction and CopyAssignment.
  template <class other_annotation_type>
  void loadFrom(const AnnotatedBlobT<other_annotation_type> &other) {
    blob = other.blob; // Copy string.
    annotation.clear();

    other_annotation_type otherBlobView(other.blob);
    annotation_type blobView(blob);

    // ByteRanges : work out the math, and create new byteRanges based on sizes
    // relative to the new string.
    const char *otherBegin = otherBlobView.data();
    const char *thisBegin = blobView.data();

    for (size_t idx = 0; idx < other.numSentences(); idx++) {
      std::vector<annotation_type> sentenceWords;
      // Obtain terminals.
      auto terminal = other.sentenceTerminals(idx);

      // Add words between terminals into container of new annotation-type.
      for (size_t wordIdx = terminal.first; wordIdx <= terminal.second;
           wordIdx++) {
        other_annotation_type word = other.wordFromFlatIdx[wordIdx];
        sentenceWords.emplace_back(
            thisBegin + (size_t)(word.data() - otherBegin), word.size());
      }
      annotation.addSentence(sentenceWords);
    }
  }
};

} // namespace bergamot
} // namespace marian

#endif //  BERGAMOT_SENTENCE_RANGES_H_
