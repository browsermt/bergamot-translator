#include "response.h"
#include "common/logging.h"
#include "data/alignment.h"
#include "sentence_ranges.h"

#include <utility>

namespace marian {
namespace bergamot {

Response::Response(std::string &&sourceBlob, SentenceRanges &&sourceRanges,
                   Histories &&histories, std::vector<Ptr<Vocab const>> &vocabs)
    : source(std::move(sourceBlob), std::move(sourceRanges)),
      histories_(std::move(histories)) {

  // Reserving length at least as much as source_ seems like a reasonable thing
  // to do to avoid reallocations.
  target.blob.reserve(source.blob.size());

  // In a first step, the decoded units (individual senteneces) are compiled
  // into a huge string. This is done by computing indices first and appending
  // to the string as each sentences are decoded.
  std::vector<std::pair<size_t, size_t>> translationRanges;
  std::vector<size_t> sentenceBegins;

  size_t offset{0};
  bool first{true};

  for (auto &history : histories_) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0]; // Expecting only one result;
    Words words = std::get<0>(result);
    auto targetVocab = vocabs.back();

    std::string decoded;
    std::vector<string_view> targetMappings;
    targetVocab->decodeWithByteRanges(words, decoded, targetMappings);

    if (first) {
      first = false;
    } else {
      target.blob += " ";
      ++offset;
    }

    sentenceBegins.push_back(translationRanges.size());
    target.blob += decoded;
    auto decodedStringBeginMarker = targetMappings.front().begin();
    for (auto &sview : targetMappings) {
      size_t startIdx = offset + sview.begin() - decodedStringBeginMarker;
      translationRanges.emplace_back(startIdx, startIdx + sview.size());
    }

    offset += decoded.size();
  }

  // Once we have the indices in translation (which might be resized a few
  // times) ready, we can prepare and store the string_view as annotations
  // instead. This is accomplished by iterating over available sentences using
  // sentenceBegin and using addSentence(...) API from Annotation.

  for (size_t i = 1; i <= sentenceBegins.size(); i++) {
    std::vector<string_view> targetMappings;
    size_t begin = sentenceBegins[i - 1];
    size_t safe_end = (i == sentenceBegins.size()) ? translationRanges.size()
                                                   : sentenceBegins[i];

    for (size_t idx = begin; idx < safe_end; idx++) {
      auto &p = translationRanges[idx];
      size_t begin_idx = p.first;
      size_t end_idx = p.second;

      const char *data = &target.blob[begin_idx];
      size_t size = end_idx - begin_idx;
      targetMappings.emplace_back(data, size);
    }

    target.annotation.addSentence(targetMappings);
  }
}
} // namespace bergamot
} // namespace marian
