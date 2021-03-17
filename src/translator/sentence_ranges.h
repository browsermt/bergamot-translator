#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include "data/types.h"
#include <cassert>
#include <utility>
#include <vector>

namespace marian {
namespace bergamot {

/// ByteRange stores indices for begin and end in a string. This can be used to
/// represent a sentence, word.
struct ByteRange {
  size_t begin_byte_offset;
  size_t end_byte_offset;
  const size_t size() const { return end_byte_offset - begin_byte_offset + 1; }
};

/// An Annotation is a collection of ByteRanges which ancillary information on a
/// blob of string.

class Annotation {
public:
  /// Allow empty construction.
  Annotation() {}

  /// clears internal containers, finds use in at least copy-assignment.
  void clear();

  /// Returns the number of sentences annotated in a blob.
  size_t numSentences() const { return sentenceBeginIds_.size(); }
  size_t numWords(size_t sentenceIdx) const {
    auto terminals = sentenceTerminalIds(sentenceIdx);
    return terminals.second - terminals.first + 1;
  }
  void addSentence(std::vector<ByteRange> &sentence);

  /// Returns indices of terminal (word) ByteRanges of a sentence corresponding
  /// to sentenceIdx
  std::pair<ByteRange, ByteRange> sentenceTerminals(size_t sentenceIdx) const;
  std::pair<size_t, size_t> sentenceTerminalIds(size_t sentenceIdx) const;

  /// Provides access to the internal word corresponding to wordIdx, for the
  /// sentence corresponding to sentenceIdx.
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
};

/// AnnotatedBlob is effectively std::string + Annotation.
/// In addition there are accessor methods which allow string_views rather than
/// ByteRanges.

struct AnnotatedBlob {
public:
  std::string blob;
  Annotation annotation;

  AnnotatedBlob() {}
  // AnnotatedBlob(std::string blob) : blob(blob){};
  AnnotatedBlob(std::string &&blob) : blob(std::move(blob)){};
  AnnotatedBlob(AnnotatedBlob &&annotatedBlob)
      : blob(std::move(annotatedBlob.blob)),
        annotation(std::move(annotatedBlob.annotation)) {}

  /// Returns the number of sentences in the annotation structure.
  const size_t numSentences() const { return annotation.numSentences(); }
  const size_t numWords(size_t sentenceIdx) const {
    return annotation.numWords(sentenceIdx);
  }

  /// Adds a sentence, used to load from SentencePiece annotations conveniently.
  /// encodeWithByteRanges and decodeWithByteRanges.
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
  string_view asStringView(const ByteRange &byteRange) const {
    const char *data = &blob[byteRange.begin_byte_offset];
    size_t size = byteRange.size();
    return string_view(data, size);
  }
};

typedef Annotation SentenceRanges;

} // namespace bergamot
} // namespace marian

#endif //  BERGAMOT_SENTENCE_RANGES_H_
