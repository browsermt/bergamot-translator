#ifndef SRC_BERGAMOT_RESPONSE_H_
#define SRC_BERGAMOT_RESPONSE_H_

#include "data/alignment.h"
#include "data/types.h"
#include "definitions.h"
#include "sentence_ranges.h"
#include "translator/beam_search.h"

#include <cassert>
#include <string>
#include <vector>

namespace marian {
namespace bergamot {

typedef marian::data::SoftAlignment SoftAlignment;
typedef marian::data::WordAlignment WordAlignment;
typedef AnnotatedBlobT<string_view> AnnotatedBlob;

class Response {
  // Response is a marian internal class (not a bergamot-translator class)
  // holding source blob of text, vector of TokenRanges corresponding to each
  // sentence in the source text blob and histories obtained from translating
  // these sentences.
  //
  // This class provides an API at a higher level in comparison to History to
  // access translations and additionally use string_view manipulations to
  // recover structure in translation from source-text's structure known through
  // reference string and string_view. As many of these computations are not
  // required until invoked, they are computed as required and stored in data
  // members where it makes sense to do so (translation,translationTokenRanges).
  //
  // Examples of such use-cases are:
  //    translation()
  //    translationInSourceStructure() TODO(@jerinphilip)
  //    alignment(idx) TODO(@jerinphilip)
  //    sentenceMappings (for bergamot-translator)

public:
  Response(std::string &&source, SentenceRanges &&sourceRanges,
           Histories &&histories,
           // Required for constructing translation and TokenRanges within
           // translation lazily.
           std::vector<Ptr<Vocab const>> &vocabs);

  // Move constructor.
  Response(Response &&other)
      : source(std::move(other.source)), target(std::move(other.target)),
        histories_(std::move(other.histories_)){};

  // Prevents CopyConstruction and CopyAssignment. sourceRanges_ is constituted
  // by string_view and copying invalidates the data member.
  Response(const Response &) = delete;
  Response &operator=(const Response &) = delete;

  const size_t size() const { return source.numSentences(); }
  const Histories &histories() const { return histories_; }

  std::vector<float> wordQuality(size_t sentenceIdx) {
    auto wordQualities = hypothesis(sentenceIdx)->tracebackWordScores();
    // Remove the eos token.
    wordQualities.pop_back();
    return wordQualities;
  }

  float sentenceQuality(size_t sentenceIdx) {
    NBestList onebest = histories_[sentenceIdx]->nBest(1);
    Result result = onebest[0]; // Expecting only one result;
    auto normalizedPathScore = std::get<2>(result);
    return normalizedPathScore;
  }

  const SoftAlignment softAlignment(size_t sentenceIdx) {
    return hypothesis(sentenceIdx)->tracebackAlignment();
  }

  const WordAlignment hardAlignment(size_t sentenceIdx, float threshold = 1.f) {
    return data::ConvertSoftAlignToHardAlign(softAlignment(sentenceIdx),
                                             threshold);
  }

  AnnotatedBlob source;
  AnnotatedBlob target;

  const std::string &translation() {
    LOG(info, "translation() will be deprecated now that target is public.");
    return target.blob;
  }

private:
  Histories histories_;

  IPtr<Hypothesis> hypothesis(size_t sentenceIdx) {
    NBestList onebest = histories_[sentenceIdx]->nBest(1);
    Result result = onebest[0]; // Expecting only one result;
    auto hyp = std::get<1>(result);
    return hyp;
  }
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_RESPONSE_H_
