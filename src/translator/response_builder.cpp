#include "response_builder.h"

namespace marian {
namespace bergamot {

void ResponseBuilder::buildQualityScores(Histories &histories,
                                         Response &response) {
  std::vector<Quality> qualityScores;
  for (auto &history : histories) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0]; // Expecting only one result;
    Words words = std::get<0>(result);
    auto hyp = std::get<1>(result);
    // Quality scores: Sequence level is obtained as normalized path scores.
    // Word level using hypothesis traceback. These are most-likely
    // logprobs.
    auto normalizedPathScore = std::get<2>(result);
    auto wordQualities = hyp->tracebackWordScores();
    wordQualities.pop_back();
    response.qualityScores.push_back(
        Quality{normalizedPathScore, wordQualities});
  }
}

void ResponseBuilder::buildAlignments(Histories &histories,
                                      Response &response) {
  for (auto &history : histories) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0]; // Expecting only one result;
    Words words = std::get<0>(result);
    // Alignments
    // TODO(jerinphilip): The following double conversion might not be
    // necessary. Hard alignment can directly be exported, but this would
    // mean WASM bindings for a structure deep within marian source.
    auto hyp = std::get<1>(result);
    auto softAlignment = hyp->tracebackAlignment();
    auto threshold = responseOptions_.alignmentThreshold;
    auto hardAlignment =
        data::ConvertSoftAlignToHardAlign(softAlignment, threshold);
    Alignment unified_alignment;
    for (auto &p : hardAlignment) {
      unified_alignment.emplace_back(Point{p.srcPos, p.tgtPos, p.prob});
    }

    response.alignments.push_back(std::move(unified_alignment));
  }
}

void ResponseBuilder::buildTranslatedText(Histories &histories,
                                          Response &response) {
  // Reserving length at least as much as source_ seems like a reasonable
  // thing to do to avoid reallocations.
  response.target.text.reserve(response.source.text.size());

  size_t offset{0};
  bool first{true};

  for (auto &history : histories) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0]; // Expecting only one result;
    Words words = std::get<0>(result);
    auto targetVocab = vocabs_->back();

    std::string decoded;
    std::vector<string_view> targetSentenceMappings;
    targetVocab->decodeWithByteRanges(words, decoded, targetSentenceMappings);

    // delimiter can be used to fill in the blanks from source as well.
    std::string delimiter;
    if (first) {
      first = false;
    } else {
      delimiter = " ";
    }

    response.target.appendSentence(delimiter, decoded, targetSentenceMappings);
  }
}

} // namespace bergamot
} // namespace marian
