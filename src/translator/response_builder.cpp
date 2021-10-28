#include "response_builder.h"

#include "response_options.h"
#include "tag_processor.h"

namespace marian {
namespace bergamot {

void ResponseBuilder::buildQualityScores(Histories &histories, Response &response) {
  qualityEstimator_.computeQualityScores(histories, response);
}

void buildTagAlignment(Response &response) {
  ABORT_IF(response.source.numSentences() != 1, "Cross-sentence tag alignment is under development at the moment.");

  std::vector<size_t> char2TokenTable(response.source.text.size(), 0);

  for (size_t sentenceId = 0; sentenceId < response.source.numSentences(); sentenceId++) {
    // Step 0: create or update char-> token table
    for (size_t s = 0; s < response.source.numWords(sentenceId); s++) {
      auto sourceByteRange = response.source.wordAsByteRange(sentenceId, s);
      for (size_t cur = sourceByteRange.begin; cur < sourceByteRange.end; cur++) {
        char2TokenTable[cur] = s;
      }
    }

    // Step 1: convert char indices to token indices conversion
    std::vector<ByteRange> tagPosSourceCharLevel = response.source.tagPositionSource_;
    std::vector<TokenIndexRange> tagPosSourceTokenLevel;
    for (size_t tagIdx = 0; tagIdx < tagPosSourceCharLevel.size(); tagIdx++) {
      size_t charIdxBegin = tagPosSourceCharLevel[tagIdx].begin;
      size_t charIdxEnd = tagPosSourceCharLevel[tagIdx].end;
      tagPosSourceTokenLevel.push_back(TokenIndexRange{char2TokenTable[charIdxBegin], char2TokenTable[charIdxEnd]});
    }

    // Step 2: build token-level tag tree
    TagTreeBuilder ttb(tagPosSourceTokenLevel);
    TagTree tagTreeSource = ttb.getTagTree();

    // Step 3: call inside-outside algorithm
    auto &alignments = response.alignments[sentenceId];
    TagProcessor tp = TagProcessor(alignments, tagTreeSource, response.source.numWords(sentenceId),
                                   response.target.numWords(sentenceId));

    int exitStatus = tp.traverseAndQuery();
    TagTree tagTreeTarget = tp.getTargetRoot();

    // Step 4: flatten the token-level tag tree for the translated text to a token-level ByteRange vector
    std::vector<TokenIndexRange> tagPosTargetTokenLevel = tagTreeTarget.flatten();

    // Step 5: convert the flattened token-level tag tree to the character level one
    std::vector<ByteRange> tagPosTargetCharLevel;
    for (TokenIndexRange tokenBound : tagPosTargetTokenLevel) {
      size_t charBegin = response.target.wordAsByteRange(sentenceId, tokenBound.begin).begin;
      size_t charEnd = response.target.wordAsByteRange(sentenceId, tokenBound.end).begin;
      tagPosTargetCharLevel.push_back(ByteRange{charBegin, charEnd});
    }

    response.tagPositionTarget = tagPosTargetCharLevel;
  }
}

void ResponseBuilder::buildAlignments(Histories &histories, Response &response) {
  for (auto &history : histories) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0];  // Expecting only one result;
    Words words = std::get<0>(result);
    // Alignments
    // TODO(jerinphilip): The following double conversion might not be
    // necessary. Hard alignment can directly be exported, but this would
    // mean WASM bindings for a structure deep within marian source.
    auto hyp = std::get<1>(result);
    auto softAlignment = hyp->tracebackAlignment();
    response.alignments.push_back(std::move(softAlignment));
    buildTagAlignment(response);
  }
}

void ResponseBuilder::buildTranslatedText(Histories &histories, Response &response) {
  // Reserving length at least as much as source_ seems like a reasonable
  // thing to do to avoid reallocations.
  response.target.text.reserve(response.source.text.size());

  for (size_t sentenceIdx = 0; sentenceIdx < histories.size(); sentenceIdx++) {
    // TODO(jerin): Change hardcode of nBest = 1

    auto &history = histories[sentenceIdx];
    NBestList onebest = history->nBest(1);

    Result result = onebest[0];  // Expecting only one result;
    Words words = std::get<0>(result);

    std::string decoded;
    std::vector<string_view> targetSentenceMappings;
    vocabs_.target()->decodeWithByteRanges(words, decoded, targetSentenceMappings, /*ignoreEOS=*/false);

    switch (responseOptions_.concatStrategy) {
      case ConcatStrategy::FAITHFUL: {
        // For each sentence, prepend the filler text between the corresponding
        // source-sentence and the source-sentence before.
        string_view pre = response.source.gap(sentenceIdx);
        response.target.appendSentence(pre, targetSentenceMappings.begin(), targetSentenceMappings.end());

        // If this is the last history to be decoded and translated-text
        // constructed, append the text till the end, which could be spaces or
        // empty.
        if (sentenceIdx + 1 == histories.size()) {
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
