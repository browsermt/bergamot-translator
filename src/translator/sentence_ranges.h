#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include "data/types.h"
#include <cassert>
#include <utility>
#include <vector>

namespace marian {
namespace bergamot {

/// ByteRange stores indices for begin and end (inclusive) in a string. Can be
/// used to represent a sentence, word.
struct ByteRange {
  size_t begin_byte_offset;
  size_t end_byte_offset;
  const size_t size() const { return end_byte_offset - begin_byte_offset + 1; }
};

/// An Annotation is a collection of ByteRanges used to denote ancillary
/// information of sentences and words on a blob of string. Annotation is meant
/// for consumption on platforms where string_view creates problems (eg: exports
/// through WASM). See AnnotatedBlob for cases where this is a non-issue.
class Annotation {
public:
  /// Annotation is constructed empty. See addSentence to populate it with
  /// annotations.
  Annotation() {}

  /// Returns the number of sentences annotated in a blob.
  size_t numSentences() const { return sentenceBeginIds_.size(); }

  /// Returns number of words in the sentece identified by sentenceIdx.
  size_t numWords(size_t sentenceIdx) const;

  /// Adds a sentences from vector<ByteRange> representation, internally doing
  /// extra book-keeping for the sentence terminal markings.
  void addSentence(std::vector<ByteRange> &sentence);

  /// Returns a ByteRange representing wordIdx in sentenceIdx
  ByteRange word(size_t sentenceIdx, size_t wordIdx) const;

  /// Returns a ByteRange representing sentence corresponding to sentenceIdx.
  ByteRange sentence(size_t sentenceIdx) const;

private:
  /// A flat storage for ByteRanges. Composed of word ByteRanges, extra
  /// information in sentenceBeginIds_ to denote sentence boundary markers as
  /// indices.
  std::vector<ByteRange> flatByteRanges_;

  /// Stores indices where sentences begin
  std::vector<size_t> sentenceBeginIds_;

  /// Returns indices of terminal (word) ByteRanges of a sentence corresponding
  /// to sentenceIdx
  std::pair<ByteRange, ByteRange> sentenceTerminals(size_t sentenceIdx) const;
  std::pair<size_t, size_t> sentenceTerminalIds(size_t sentenceIdx) const;
};

/// AnnotatedBlob is effectively std::string blob + Annotation, providing the
/// following additional desiderata.
///
/// 1. Access to processed string_views for convenience rather than ByteRanges
/// (which only provides index information).
///
/// 2. Transparently convert string_views into ByteRanges for the Annotation
/// referring to the blob bound by this structure.
///
/// 3. Bind the blob and annotations together, to move around as a meaningful
/// unit.

struct AnnotatedBlob {
public:
  std::string blob;      ///< Blob of string elements in annotation refers to.
  Annotation annotation; ///< sentence and (sub-) word annotations.

  /// Construct an empty AnnotatedBlob. This is useful when the target string or
  /// ByteRanges are not known yet, but the public members can be used to
  /// populate it. One use-case, when translated-text is created decoding from
  /// histories and the ByteRanges only known after the string has been
  /// constructed.
  AnnotatedBlob() {}

  /// Construct moving in a string (for efficiency purposes, copying string
  /// constructor is disallowed).
  AnnotatedBlob(std::string &&blob) : blob(std::move(blob)){};

  AnnotatedBlob(AnnotatedBlob &&annotatedBlob)
      : blob(std::move(annotatedBlob.blob)),
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

  string_view word(size_t sentenceIdx, size_t wordIdx) const;
  string_view sentence(size_t sentenceIdx) const;

  ByteRange wordAsByteRange(size_t sentenceIdx, size_t wordIdx) const;
  ByteRange sentenceAsByteRange(size_t sentenceIdx) const;

private:
  string_view asStringView(const ByteRange &byteRange) const;
};

} // namespace bergamot
} // namespace marian

#endif //  BERGAMOT_SENTENCE_RANGES_H_
