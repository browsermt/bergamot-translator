#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include "data/types.h"
#include <cassert>
#include <utility>
#include <vector>

namespace marian {
namespace bergamot {

/// ByteRange stores indices for half-interval [begin, end) in a string. Can be
/// used to represent a sentence, word.
struct ByteRange {
  size_t begin;
  size_t end;
  const size_t size() const { return end - begin; }
};

/// An Annotation is a collection of ByteRanges used to denote ancillary
/// information of sentences and words on a text of string. Annotation is meant
/// for consumption on platforms where string_view creates problems (eg: exports
/// through WASM). See AnnotatedText for cases where this is a non-issue.
class Annotation {
public:
  /// Annotation is constructed empty. See addSentence to populate it with
  /// annotations.
  Annotation() {
    // The -1-th sentence ends at 0.
    sentenceEndIds_.push_back(0);
  }

  /// Returns the number of sentences annotated in a text.
  size_t numSentences() const { return sentenceEndIds_.size() - 1; }

  /// Returns number of words in the sentece identified by sentenceIdx.
  size_t numWords(size_t sentenceIdx) const;

  /// Adds a sentences from vector<ByteRange> representation, internally doing
  /// extra book-keeping for the sentence terminal markings. Sentences are
  /// expected to be added in order as they occur in text.
  void addSentence(std::vector<ByteRange> &sentence);

  /// Returns a ByteRange representing wordIdx in sentenceIdx
  ByteRange word(size_t sentenceIdx, size_t wordIdx) const;

  /// Returns a ByteRange representing sentence corresponding to sentenceIdx.
  ByteRange sentence(size_t sentenceIdx) const;

private:
  /// A flat storage for ByteRanges. Composed of word ByteRanges, extra
  /// information in sentenceEndIds_ to denote sentence boundary markers as
  /// indices.
  std::vector<ByteRange> flatByteRanges_;

  /// Stores indices onto flatByteRanges_ of where sentences end (not inclusive,
  /// aligned with C++ half interval notions). There is a 0 marker to simplify
  /// sources, indicating where the -1-th sentence ends.
  std::vector<size_t> sentenceEndIds_;
};

/// AnnotatedText is effectively std::string text + Annotation, providing the
/// following additional desiderata.
///
/// 1. Access to processed string_views for convenience rather than ByteRanges
/// (which only provides index information).
///
/// 2. Transparently convert string_views into ByteRanges for the Annotation
/// referring to the text bound by this structure.
///
/// 3. Bind the text and annotations together, to move around as a meaningful
/// unit.

struct AnnotatedText {
public:
  std::string text;      ///< Blob of string elements in annotation refers to.
  Annotation annotation; ///< sentence and (sub-) word annotations.

  /// Construct an empty AnnotatedText. This is useful when the target string or
  /// ByteRanges are not known yet, but the public members can be used to
  /// populate it. One use-case, when translated-text is created decoding from
  /// histories and the ByteRanges only known after the string has been
  /// constructed.
  AnnotatedText() {}

  /// Construct moving in a string (for efficiency purposes, copying string
  /// constructor is disallowed).
  AnnotatedText(std::string &&text) : text(std::move(text)){};

  AnnotatedText(AnnotatedText &&annotatedBlob)
      : text(std::move(annotatedBlob.text)),
        annotation(std::move(annotatedBlob.annotation)) {}

  /// Returns the number of sentences in the annotation structure.
  const size_t numSentences() const { return annotation.numSentences(); }

  /// Returns number of words in the sentece identified by sentenceIdx.
  const size_t numWords(size_t sentenceIdx) const {
    return annotation.numWords(sentenceIdx);
  }

  /// Adds a sentence, used to load from SentencePiece annotations conveniently.
  void addSentence(std::vector<string_view> &wordRanges);

  /// Adds a sentence between two iterators, often useful while constructing
  /// from parts of a container.
  void addSentence(std::vector<string_view>::iterator begin,
                   std::vector<string_view>::iterator end);

  /// Returns a string_view representing wordIdx in sentenceIdx
  string_view word(size_t sentenceIdx, size_t wordIdx) const;

  /// Returns a string_view representing sentence corresponding to sentenceIdx.
  string_view sentence(size_t sentenceIdx) const;

  /// Returns a ByteRange representing wordIdx in sentenceIdx
  ByteRange wordAsByteRange(size_t sentenceIdx, size_t wordIdx) const;

  /// Returns a ByteRange representing sentence corresponding to sentenceIdx.
  ByteRange sentenceAsByteRange(size_t sentenceIdx) const;

private:
  string_view asStringView(const ByteRange &byteRange) const;
};

} // namespace bergamot
} // namespace marian

#endif //  BERGAMOT_SENTENCE_RANGES_H_
