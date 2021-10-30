#include "response_builder.h"

#include "response_options.h"

namespace marian {
namespace bergamot {

void ResponseBuilder::buildQualityScores(Histories &histories, Response &response) {
  qualityEstimator_.computeQualityScores(histories, response);
}

void buildTagAlignment(Response &response) {
  // Step 0: set up look-up tables
  ABORT_IF(response.source.numSentences() == 0, "No sentence in source");
  std::vector<size_t> tokenOffsetTable(response.source.numSentences(), 0);
  size_t numSourceTokens = response.source.numWords(0);
  for (size_t i = 1; i < response.source.numSentences(); i++) {
    tokenOffsetTable[i] = tokenOffsetTable[i - 1] + response.source.numWords(i);
    numSourceTokens += response.source.numWords(i);
  }
  ABORT_IF(numSourceTokens == 0, "No token in source");

  std::vector<size_t> targetTokenSidTable;
  std::vector<size_t> targetTokenTidTable;
  size_t numTargetTokens = 0;
  for (size_t i = 0; i < response.target.numSentences(); i++) {
    for (size_t j = 0; j < response.target.numWords(i); j++) {
      targetTokenSidTable.push_back(i);
      targetTokenTidTable.push_back(j);
    }
    numTargetTokens += response.target.numWords(i);
  }

  std::vector<size_t> char2TokenTable(response.source.text.size(), 0);
  std::vector<bool> char2TokenTableValid(response.source.text.size(), 0);
  for (size_t sentenceId = 0; sentenceId < response.source.numSentences(); sentenceId++) {
    // Step 0: create or update char-> token table
    for (size_t s = 0; s < response.source.numWords(sentenceId); s++) {
      auto sourceByteRange = response.source.wordAsByteRange(sentenceId, s);
      for (size_t cur = sourceByteRange.begin; cur < sourceByteRange.end; cur++) {
        char2TokenTable[cur] = s + tokenOffsetTable[sentenceId];
        char2TokenTableValid[cur] = true;
      }
    }
  }

  for (size_t i = 0; i < char2TokenTable.size(); i++) {
    if (i > 0 && !char2TokenTableValid[i]) char2TokenTable[i] = char2TokenTable[i - 1] + 1;
  }

  // Step 1: convert char indices to token indices
  std::vector<ByteRange> &tagPosSourceCharLevel = response.source.tagPositions;
  std::vector<TokenIndexRange> tagPosSourceTokenLevel;
  for (size_t tagIdx = 0; tagIdx < tagPosSourceCharLevel.size(); tagIdx++) {
    size_t charIdxBegin = tagPosSourceCharLevel[tagIdx].begin;
    size_t charIdxEnd = tagPosSourceCharLevel[tagIdx].end;
    tagPosSourceTokenLevel.push_back(TokenIndexRange{char2TokenTable[charIdxBegin], char2TokenTable[charIdxEnd]});
  }

  // Step 2: scale the positions
  double ratio = (double)numTargetTokens / numSourceTokens;
  std::vector<TokenIndexRange> tagPosTargetTokenLevel;
  for (size_t i = 0; i < tagPosSourceTokenLevel.size(); i++) {
    size_t tokenBegin = (size_t)(tagPosSourceTokenLevel[i].begin * ratio);
    size_t tokenEnd = (size_t)(tagPosSourceTokenLevel[i].end * ratio);
    tagPosTargetTokenLevel.push_back(ByteRange{tokenBegin, tokenEnd});
  }

  // Step 3: convert token-level tags to the character level one
  std::vector<ByteRange> tagPosTargetCharLevel;
  for (TokenIndexRange tokenBound : tagPosTargetTokenLevel) {
    size_t charBeginSid = targetTokenSidTable[tokenBound.begin];
    size_t charBeginTid = targetTokenTidTable[tokenBound.begin];
    size_t charEndSid = targetTokenSidTable[tokenBound.end];
    size_t charEndTid = targetTokenTidTable[tokenBound.end];
    size_t charBegin = response.target.wordAsByteRange(charBeginSid, charBeginTid).begin;
    size_t charEnd = response.target.wordAsByteRange(charEndSid, charEndTid).begin;
    tagPosTargetCharLevel.push_back(ByteRange{charBegin, charEnd});
  }

  response.target.tagPositions = std::move(tagPosTargetCharLevel);
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
    auto threshold = responseOptions_.alignmentThreshold;
    auto hardAlignment = data::ConvertSoftAlignToHardAlign(softAlignment, threshold);
    Alignment unified_alignment;
    for (auto &p : hardAlignment) {
      unified_alignment.emplace_back(Point{p.srcPos, p.tgtPos, p.prob});
    }

    response.alignments.push_back(std::move(unified_alignment));
    if (!response.source.tagPositions.empty()) buildTagAlignment(response);
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
