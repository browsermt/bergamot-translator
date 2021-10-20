#include "response.h"

#include "annotation.h"
#include "definitions.h"

namespace marian::bergamot {

// We're marginalizing q out of p(s | q) x p( q | t). However, we have different representations of q on source side to
// intermediate - p(s_i | q_j) and intermediate to target side - p(q'_j' | t_k).
//
// The matrix p(q'_j' | t_k) is rewritten into p(q_j | t_k) by means of spreading the probability in the former over
// bytes and collecting it at the ranges specified by latter, using a two pointer accumulation strategy.
Alignment transferThroughCharacters(const std::vector<ByteRange> &sourceSidePivots,
                                    const std::vector<ByteRange> &targetSidePivots,
                                    const Alignment &targetGivenPivots) {
  // Initialize an empty alignment matrix.
  Alignment remapped(targetGivenPivots.size(), std::vector<float>(sourceSidePivots.size(), 0.0f));

  for (size_t sq = 0, qt = 0; sq < sourceSidePivots.size() && qt < targetSidePivots.size();
       /*each branch inside increments either sq or qt or both, therefore the loop terminates */) {
    auto &sourceSidePivot = sourceSidePivots[sq];
    auto &targetSidePivot = targetSidePivots[qt];
    if (sourceSidePivot.begin == targetSidePivot.begin && sourceSidePivot.end == targetSidePivot.end) {
      for (size_t t = 0; t < targetGivenPivots.size(); t++) {
        remapped[t][sq] += targetGivenPivots[t][qt];
      }

      // Perfect match, move pointer from both.
      sq++, qt++;
    } else {
      // Do we have overlap?
      size_t left = std::max(targetSidePivot.begin, sourceSidePivot.begin);
      size_t right = std::min(targetSidePivot.end, sourceSidePivot.end);

      assert(left <= right);  // there should be overlap.

      size_t charCount = right - left;
      size_t probSpread = targetSidePivot.size();
      float fraction = probSpread == 0 ? 1.0f : static_cast<float>(charCount) / static_cast<float>(probSpread);
      for (size_t t = 0; t < targetGivenPivots.size(); t++) {
        remapped[t][sq] += fraction * targetGivenPivots[t][qt];
      }

      // Which one is ahead? sq or qt or both end at same point?
      if (sourceSidePivot.end == targetSidePivot.end) {
        sq++;
        qt++;
      } else if (sourceSidePivot.end > targetSidePivot.end) {
        qt++;
      } else {  // sourceSidePivot.end < targetSidePivot.end
        sq++;
      }
    }
  }

  // At the end, assert what we have is a valid probability distribution.
#ifdef DEBUG
  for (size_t t = 0; t < targets.size(); t++) {
    float sum = 0.0f;
    for (size_t q = 0; q < sourceSidePivots.size(); q++) {
      sum += remapped[t][q];
    }

    const float EPS = 1e-6;
    assert(std::abs(sum - 1.0f) < EPS);
  }
#endif

  return remapped;
}

std::vector<Alignment> remapAlignments(const Response &first, const Response &second) {
  std::vector<Alignment> alignments;
  for (size_t sentenceId = 0; sentenceId < first.source.numSentences(); sentenceId++) {
    const Alignment &sourceGivenPivots = first.alignments[sentenceId];
    const Alignment &targetGivenPivots = second.alignments[sentenceId];

    // TODO: Allow range iterators and change algorithm, directly tapping into AnnotatedText
    // Extracts ByteRanges corresponding to a words constituting a sentence from an annotation.
    auto extractWordByteRanges = [](const AnnotatedText &annotatedText,
                                    size_t sentenceId) -> std::vector<marian::bergamot::ByteRange> {
      size_t N = annotatedText.numWords(sentenceId);
      std::vector<ByteRange> output;

      for (size_t i = 0; i < N; i++) {
        output.push_back(annotatedText.wordAsByteRange(sentenceId, i));
      }
      return output;
    };

    auto sourceSidePivots = extractWordByteRanges(first.target, sentenceId);
    auto targetSidePivots = extractWordByteRanges(second.source, sentenceId);

    // Reintrepret probability p(q'_j' | t_k) as p(q_j | t_k)
    Alignment remappedPivotGivenTargets =
        transferThroughCharacters(sourceSidePivots, targetSidePivots, targetGivenPivots);

    // Marginalize out q_j.
    // p(s_i | t_k) = \sum_{j} p(s_i | q_j) x p(q_j | t_k)
    size_t sourceTokenCount = first.source.numWords(sentenceId);
    size_t targetTokenCount = second.target.numWords(sentenceId);
    Alignment output(targetTokenCount, std::vector<float>(sourceTokenCount, 0.0f));
    for (size_t ids = 0; ids < sourceTokenCount; ids++) {
      for (size_t idq = 0; idq < sourceSidePivots.size(); idq++) {
        for (size_t idt = 0; idt < targetTokenCount; idt++) {
          // Matrices are of form p(s | t) = P[t][s], hence idq appears on the extremes.
          output[idt][ids] += sourceGivenPivots[idq][ids] * remappedPivotGivenTargets[idt][idq];
        }
      }
    }

    alignments.push_back(output);
  }
  return alignments;
}

}  // namespace marian::bergamot
