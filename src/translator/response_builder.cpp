#include "response_builder.h"

#include "response_options.h"

namespace marian {
namespace bergamot {

void ResponseBuilder::buildQualityScores(const ProcessedRequestSentences &processedRequestSentences,
                                         Response &response) {
  qualityEstimator_.computeQualityScores(processedRequestSentences, response);
}

void ResponseBuilder::buildAlignments(const ProcessedRequestSentences &processedRequestSentences, Response &response) {
  // There are no means to test this works due to tests being removed. TagTree will have to take over after cache,
  // tentatively.
  /*
  for (auto &processedRequestSentence : processedRequestSentences) {
    auto softAlignment = processedRequestSentence.softAlignment();
    auto threshold = responseOptions_.alignmentThreshold;
    auto hardAlignment = data::ConvertSoftAlignToHardAlign(softAlignment, threshold);
    Alignment unified_alignment;
    for (auto &p : hardAlignment) {
      unified_alignment.emplace_back(Point{p.srcPos, p.tgtPos, p.prob});
    }

    response.alignments.push_back(std::move(unified_alignment));
  }
  */
}

void ResponseBuilder::buildTranslatedText(const ProcessedRequestSentences &processedRequestSentences,
                                          Response &response) {
  // Reserving length at least as much as source_ seems like a reasonable
  // thing to do to avoid reallocations.
  response.target.text.reserve(response.source.text.size());

  for (size_t sentenceIdx = 0; sentenceIdx < processedRequestSentences.size(); sentenceIdx++) {
    Words words;

    auto wordsSpan = processedRequestSentences[sentenceIdx].words();
    for (auto p = wordsSpan.cbegin(); p != wordsSpan.cend(); p++) {
      words.push_back(*p);
    }

    std::string decoded;
    std::vector<string_view> targetSentenceMappings;
    vocabs_.target()->decodeWithByteRanges(words, decoded, targetSentenceMappings, /*ignoreEOS=*/false);

    switch (responseOptions_.concatStrategy) {
      case ConcatStrategy::FAITHFUL: {
        // For each sentence, prepend the filler text between the corresponding
        // source-sentence and the source-sentence before.
        string_view pre = response.source.gap(sentenceIdx);
        response.target.appendSentence(pre, targetSentenceMappings.begin(), targetSentenceMappings.end());

        // If this is the last processedRequestSentence to be decoded and translated-text
        // constructed, append the text till the end, which could be spaces or
        // empty.
        if (sentenceIdx + 1 == processedRequestSentences.size()) {
          response.target.appendEndingWhitespace(response.source.gap(sentenceIdx + 1));
        }
        break;
      }
      case ConcatStrategy::SPACE: {
        string_view delimiter = (sentenceIdx == 0) ? "" : " ";
        response.target.appendSentence(delimiter, targetSentenceMappings.begin(), targetSentenceMappings.end());
        break;
      }

      default:
        ABORT("Unknown concat-strategy");
    }
  }
}

}  // namespace bergamot
}  // namespace marian
