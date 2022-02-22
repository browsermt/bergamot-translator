#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include <cassert>
#include <utility>
#include <vector>

#include "data/types.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

/// Annotation expresses sentence and token boundary information as ranges of
/// bytes in a string, but does not itself own the string.
///
/// See also AnnotatedText, which owns Annotation and the string. AnnotatedText
/// wraps these ByteRange functions to provide a string_view interface.
///
/// Text is divided into gaps (whitespace between sentences) and sentences like
/// so:
///   gap sentence gap sentence gap
/// Because gaps appear at the beginning and end of the text, there's always
/// one more gap than there are sentences.
///
/// The entire text is a unbroken sequence of tokens (i.e. the end of a token
/// is the beginning of the next token).  A gap is exactly one token containing
/// whatever whitespace is between the sentences.  A sentence is a sequence of
/// tokens.
///
/// Since we are using SentencePiece, a token can include whitespace.  The term
/// "word" is used, somewhat incorrectly, as a synonym of token.
///
/// A gap can be empty (for example there may not have been whitespace at the
/// beginning).  A sentence can also be empty (typically the translation system
/// produced empty output).  That's fine, these are just empty ranges as you
/// would expect.
class Annotation {
 public:
  /// Initially an empty string.  Populated by AnnotatedText.
  Annotation() {
    token_begin_.push_back(0);
    token_begin_.push_back(0);
    gap_.push_back(0);
  }

  size_t numSentences() const { return gap_.size() - 1; }

  /// Returns number of words in the sentence identified by `sentenceIdx`.
  size_t numWords(size_t sentenceIdx) const {
    return gap_[sentenceIdx + 1] - gap_[sentenceIdx] - 1 /* minus the gap */;
  }

  /// Returns a ByteRange representing `wordIdx` in sentence indexed by
  /// `sentenceIdx`. `wordIdx` follows 0-based indexing, and should be less than
  /// `.numWords()` for `sentenceIdx` for defined behaviour.
  ByteRange word(size_t sentenceIdx, size_t wordIdx) const {
    size_t tokenIdx = gap_[sentenceIdx] + 1 + wordIdx;
    return ByteRange{token_begin_[tokenIdx], token_begin_[tokenIdx + 1]};
  }

  /// Returns a ByteRange representing sentence corresponding to `sentenceIdx`.
  /// `sentenceIdx` follows 0-based indexing, and behaviour is defined only when
  /// less than `.numSentences()`.
  ByteRange sentence(size_t sentenceIdx) const {
    return ByteRange{
        token_begin_[gap_[sentenceIdx] + 1], /*end of whitespace before */
        token_begin_[gap_[sentenceIdx + 1]]  /*beginning of whitespace after */
    };
  }

  ByteRange gap(size_t gapIdx) const {
    size_t tokenIdx = gap_[gapIdx];
    return ByteRange{token_begin_[tokenIdx], token_begin_[tokenIdx + 1]};
  }

 private:
  friend class AnnotatedText;
  /// Map from token index to byte offset at which it begins.  Token i is:
  ///   [token_begin_[i], token_begin_[i+1])
  /// The vector is padded so that these indices are always valid, even at the
  /// end.  So tokens_begin_.size() is the number of tokens plus 1.
  std::vector<size_t> token_begin_;

  /// Indices of tokens that correspond to gaps between sentences.  These are
  /// indices into token_begin_.
  /// Gap g is byte range:
  ///   [token_begin_[gap_[w]], token_begin_[gap_[w]+1])
  /// Sentence s is byte range:
  ///   [token_begin_[gap_[s]+1], token_begin_[gap_[s+1]])
  /// A sentence does not include whitespace at the beginning or end.
  ///
  /// gap_.size() == numSentences() + 1.
  ///
  /// Example: empty text "" -> just an empty gap.
  /// token_begin_ = {0, 0};
  /// gap_ = {0};
  ///
  /// Example: only space " " -> just a gap containing the space.
  /// token_begin_ = {0, 1};
  /// gap_ = {0};
  ///
  /// Example: one token "hi" -> empty gap, sentence with one token, empty gap
  /// token_begin_ = {0, 0, 2, 2};
  /// gap_ = {0, 2};
  std::vector<size_t> gap_;
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
  std::string text;       ///< Blob of string elements in annotation refers to.
  Annotation annotation;  ///< sentence and (sub-) word annotations.

  /// Construct an empty AnnotatedText. This is useful when the target string or
  /// ByteRanges are not known yet, but the public members can be used to
  /// populate it. One use-case, when translated-text is created decoding from
  /// histories and the ByteRanges only known after the string has been
  /// constructed.
  AnnotatedText() {}

  /// Construct moving in a string (for efficiency purposes, copying string
  /// constructor is disallowed).
  AnnotatedText(std::string &&text);

  /// Appends a sentence to the existing text and transparently rebases
  /// string_views.  Since this tracks only prefix, remember
  /// appendEndingWhitespace.
  /// The string_views must not already be in text.
  void appendSentence(string_view prefix, std::vector<string_view>::iterator tokens_begin,
                      std::vector<string_view>::iterator tokens_end);

  /// Append the whitespace at the end of input. string_view must not be in
  /// text.
  void appendEndingWhitespace(string_view whitespace);

  /// Record the existence of a sentence that is already in text.  The
  /// iterators are over string_views for each token that must be in text
  /// already.  This function must be called to record sentences in order.
  /// Normally the beginning of the sentence can be inferred from
  /// tokens_begin->data() but the tokens could be empty, so sentence_begin is
  /// required to know where the sentence is.
  void recordExistingSentence(std::vector<string_view>::iterator tokens_begin,
                              std::vector<string_view>::iterator tokens_end, const char *sentence_begin);

  /// Returns the number of sentences in the annotation structure.
  const size_t numSentences() const { return annotation.numSentences(); }

  /// Returns number of words in the sentece identified by sentenceIdx.
  const size_t numWords(size_t sentenceIdx) const { return annotation.numWords(sentenceIdx); }

  /// Returns a string_view representing wordIdx in sentenceIdx
  string_view word(size_t sentenceIdx, size_t wordIdx) const {
    return asStringView(annotation.word(sentenceIdx, wordIdx));
  }

  /// Returns a string_view representing sentence corresponding to sentenceIdx.
  string_view sentence(size_t sentenceIdx) const { return asStringView(annotation.sentence(sentenceIdx)); }

  /// Returns the string_view of the gap between two sentences in the container.
  ///
  /// More precisely where `i = sentenceIdx, N = numSentences()` for brevity:
  ///
  /// * For `i = 0`: The gap between the start of text and the 0th sentence.
  /// * For `i = 1...N-1`, returns the text comprising of the gap
  ///   between the `i`-th and `i+1`-th sentence.
  /// * For `i = N`, the gap between the last (N-1th) sentence and end of
  ///   text.
  /// @param sentenceIdx: Can be between `[0, numSentences()]`.
  string_view gap(size_t sentenceIdx) const { return asStringView(annotation.gap(sentenceIdx)); }

  /// Returns a ByteRange representing wordIdx in sentenceIdx
  ByteRange wordAsByteRange(size_t sentenceIdx, size_t wordIdx) const { return annotation.word(sentenceIdx, wordIdx); }

  /// Returns a ByteRange representing sentence corresponding to sentenceIdx.
  ByteRange sentenceAsByteRange(size_t sentenceIdx) const { return annotation.sentence(sentenceIdx); }

  /// Utility function to call `fun` on each word (subword token effectively) in
  /// an `AnnotatedText`. `fun` is called with the `ByteRange`, the `string_view`
  /// with the word, and a `bool` to indicate whether it is the last word in the
  /// `AnnotatedText`, which is also the ending whitespace slot of AnnotatedText.
  template <typename Fun>
  AnnotatedText apply(Fun fun) const {
    AnnotatedText out;

    for (size_t sentenceIdx = 0; sentenceIdx < numSentences(); ++sentenceIdx) {
      std::string sentence;
      std::vector<ByteRange> tokens;

      std::string prefix = fun(annotation.gap(sentenceIdx), gap(sentenceIdx), false);

      for (size_t wordIdx = 0; wordIdx < numWords(sentenceIdx); ++wordIdx) {
        std::string token = fun(wordAsByteRange(sentenceIdx, wordIdx), word(sentenceIdx, wordIdx), false);
        tokens.push_back(ByteRange{sentence.size(), sentence.size() + token.size()});
        sentence += token;
      }

      // Convert our ByteRanges to string_views since that's what appendSentence
      // expects
      std::vector<marian::string_view> views(tokens.size());
      std::transform(tokens.begin(), tokens.end(), views.begin(), [&](ByteRange const &range) {
        return marian::string_view(sentence.data() + range.begin, range.size());
      });

      out.appendSentence(prefix, views.begin(), views.end());
    }

    out.appendEndingWhitespace(fun(annotation.gap(numSentences()), gap(numSentences()), true));

    return out;
  }

 private:
  string_view asStringView(const ByteRange &byteRange) const {
    return string_view(text.data() + byteRange.begin, byteRange.size());
  }
};

}  // namespace bergamot
}  // namespace marian

#endif  //  BERGAMOT_SENTENCE_RANGES_H_
