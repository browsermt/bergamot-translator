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
                                    const Alignment &pivotGivenTargets) {
  // Initialize an empty alignment matrix.
  Alignment remapped(pivotGivenTargets.size(), std::vector<float>(sourceSidePivots.size(), 0.0f));

  size_t sq, qt;
  for (sq = 0, qt = 0; sq < sourceSidePivots.size() && qt < targetSidePivots.size();
       /*each branch inside increments either sq or qt or both, therefore the loop terminates */) {
    auto &sourceSidePivot = sourceSidePivots[sq];
    auto &targetSidePivot = targetSidePivots[qt];
    if (sourceSidePivot.begin == targetSidePivot.begin && sourceSidePivot.end == targetSidePivot.end) {
      for (size_t t = 0; t < pivotGivenTargets.size(); t++) {
        remapped[t][sq] += pivotGivenTargets[t][qt];
      }

      // Perfect match, move pointer from both.
      sq++, qt++;
    } else {
      // Do we have overlap?
      size_t left = std::max(targetSidePivot.begin, sourceSidePivot.begin);
      size_t right = std::min(targetSidePivot.end, sourceSidePivot.end);

      assert(left < right);  // there should be overlap.

      size_t charCount = right - left;
      size_t probSpread = targetSidePivot.size();
      for (size_t t = 0; t < pivotGivenTargets.size(); t++) {
        double logprob = std::log(static_cast<double>(charCount)) +
                         std::log(static_cast<double>(pivotGivenTargets[t][qt])) -
                         std::log(static_cast<double>(probSpread));
        remapped[t][sq] += std::exp(logprob);
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

  assert(sq == sourceSidePivots.size());  // Nothing can be done here, this should not happen.

  while (qt < targetSidePivots.size()) {
    // There is a case of EOS not being predicted. In this case the two pointer algorithm will fail. The just author
    // will redistribute the surplus among subjects.

    assert(qt.size() == 0);  // assert in DEBUG, that this is only EOS.
    for (size_t t = 0; t < pivotGivenTargets.size(); t++) {
      float gift = pivotGivenTargets[t][qt] / sourceSidePivots.size();
      for (size_t sq = 0; sq < sourceSidePivots.size(); sq++) {
        remapped[t][sq] += gift;
      }
    }

    qt++;
  }

#ifdef DEBUG
  // The following sanity check ensures when DEBUG is enabled that we have successfully transferred all probabily mass
  // available over pivot tokens given a target token in our original input to the new remapped representation.
  //
  // It's been discovered that floating point arithmetic before we get the Alignment matrix can have values such that
  // the distribution does not sum upto 1.
  const float EPS = 1e-6;
  for (size_t t = 0; t < pivotGivenTargets.size(); t++) {
    float sum = 0.0f, expectedSum = 0.0f;
    for (size_t qt = 0; qt < targetSidePivots.size(); qt++) {
      expectedSum += pivotGivenTargets[t][qt];
    }
    for (size_t sq = 0; sq < sourceSidePivots.size(); sq++) {
      sum += remapped[t][sq];
    }
    std::cerr << fmt::format("Sum @ token {} = {} to be compared with expected {}.", t, sum, expectedSum) << std::endl;
    ABORT_IF(std::abs(sum - expectedSum) > EPS, "Haven't accumulated probabilities, re-examine");
  }
#endif  // DEBUG

  return remapped;
}

std::vector<Alignment> remapAlignments(const Response &first, const Response &second) {
  std::vector<Alignment> alignments;
  for (size_t sentenceId = 0; sentenceId < first.source.numSentences(); sentenceId++) {
    const Alignment &sourceGivenPivots = first.alignments[sentenceId];
    const Alignment &pivotGivenTargets = second.alignments[sentenceId];

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
        transferThroughCharacters(sourceSidePivots, targetSidePivots, pivotGivenTargets);

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
