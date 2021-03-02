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

  size_t offset{0};
  bool first{true};

  for (auto &history : histories_) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0]; // Expecting only one result;
    Words words = std::get<0>(result);
    auto targetVocab = vocabs.back();
    std::string decoded = targetVocab->decode(words);
    if (first) {
      first = false;
    } else {
      target.blob += " ";
      ++offset;
    }

    target.blob += decoded;
    translationRanges.emplace_back(offset, decoded.size());
    offset += decoded.size();
  }

  // Once the entire string is constructed, there are no further possibility of
  // reallocation in the string's storage, the indices are converted into
  // string_views.

  for (auto &range : translationRanges) {
    // TODO(@jerinphilip):  Currently considers target tokens as whole text.
    // Needs to be further enhanced in marian-dev to extract alignments.
    std::vector<string_view> targetMappings;

    const char *begin = &target.blob[range.first];
    targetMappings.emplace_back(begin, range.second);
    target.annotation.addSentence(targetMappings);
  }
}

} // namespace bergamot
} // namespace marian
