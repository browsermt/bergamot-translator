#ifndef SRC_BERGAMOT_RESPONSE_H_
#define SRC_BERGAMOT_RESPONSE_H_

#include <cassert>
#include <string>
#include <vector>

#include "annotation.h"
#include "data/alignment.h"
#include "data/types.h"
#include "definitions.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

typedef std::vector<std::vector<float>> Alignment;

/// Response holds AnnotatedText(s) of source-text and translated text,
/// alignment information between source and target sub-words and sentences.
///
/// AnnotatedText provides an API to access markings of (sub)-word and
/// sentences boundaries, which are required to interpret Quality and
/// Alignment (s) at the moment.
struct Response {
  /// SentenceQualityScore  contains the quality data of a given translated sentence.
  /// It includes the confidence (proxied by log probabilities) of each decoded word
  /// (higher logprobs imply better-translated words), the ByteRanges of each term,
  /// and logprobs of the whole sentence, represented as the mean word scores.
  struct SentenceQualityScore {
    /// Quality score of each translated word
    std::vector<float> wordScores;
    /// Position of start and end token of each word in the translated text
    std::vector<SubwordRange> wordRanges;
    /// Whole sentence quality score (it is composed by the mean of its words)
    float sentenceScore = 0.0;
  };

  /// Convenience function to obtain number of units translated. Same as
  /// `.source.numSentences()` and `.target.numSentences().` The processing of a
  /// text of into sentences are handled internally, and this information can be
  /// used to iterate through meaningful units of translation for which
  /// alignment and quality information are available.
  const size_t size() const { return source.numSentences(); }

  /// source text and annotations of (sub-)words and sentences.
  AnnotatedText source;

  /// translated text and annotations of (sub-)words and sentences.
  AnnotatedText target;

  /// logprob of each word and  the total sequence (sentence)
  /// normalized by length, for each sentence processed by the translator.
  /// Indices correspond to ranges accessible through respective Annotation on
  /// source or target.
  std::vector<SentenceQualityScore> qualityScores;

  /// Alignments between source and target. This is a collection of dense matrices providing
  ///    P[t][s] = p(source-token s  | target token t)
  /// with an alignment matrix for each sentence.
  std::vector<std::vector<std::vector<float>>> alignments;

  /// Returns the source sentence (in terms of byte range) corresponding to sentenceIdx.
  ///
  /// @param [in] sentenceIdx: The index representing the sentence where 0 <= sentenceIdx < Response::size()
  ByteRange getSourceSentenceAsByteRange(size_t sentenceIdx) const { return source.sentenceAsByteRange(sentenceIdx); }

  /// Returns the translated sentence (in terms of byte range) corresponding to sentenceIdx.
  ///
  /// @param [in] sentenceIdx: The index representing the sentence where 0 <= sentenceIdx < Response::size()
  ByteRange getTargetSentenceAsByteRange(size_t sentenceIdx) const { return target.sentenceAsByteRange(sentenceIdx); }

  const std::string &getOriginalText() const { return source.text; }

  const std::string &getTranslatedText() const { return target.text; }
};

std::vector<Alignment> remapAlignments(const Response &first, const Response &second);

std::vector<ByteRange> getWordByteRanges(Response const &response, size_t sentenceIdx);

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_RESPONSE_H_
