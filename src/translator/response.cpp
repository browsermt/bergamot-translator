#include "response.h"
#include "sentence_ranges.h"
#include "common/logging.h"
#include "data/alignment.h"

#include <utility>

namespace marian {
namespace bergamot {

Response::Response(std::string &&source, SentenceRanges &&sourceRanges,
                   Histories &&histories, std::vector<Ptr<Vocab const>> &vocabs)
    : source_(std::move(source)), sourceRanges_(std::move(sourceRanges)),
      histories_(std::move(histories)), vocabs_(&vocabs) {}

void Response::move(std::string &source, std::string &translation,
                    SentenceMappings &sentenceMappings) {

  // Construct required stuff first.
  constructTranslation();
  constructSentenceMappings(sentenceMappings);

  // Move content out.
  source = std::move(source_);
  translation = std::move(translation_);

  // The above assignment expects source, target be moved.
  // which makes the following invalid, hence required to be cleared.
  sourceRanges_.clear();
  targetRanges_.clear();
  histories_.clear();
}

void Response::constructTranslation() {
  if (translationConstructed_) {
    return;
  }

  // Reserving length at least as much as source_ seems like a reasonable thing
  // to do to avoid reallocations.
  translation_.reserve(source_.size());

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
    auto targetVocab = vocabs_->back();
    std::string decoded = targetVocab->decode(words);
    if (first) {
      first = false;
    } else {
      translation_ += " ";
      ++offset;
    }

    translation_ += decoded;
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

    const char *begin = &translation_[range.first];
    targetMappings.emplace_back(begin, range.second);
    targetRanges_.addSentence(targetMappings);
  }

  translationConstructed_ = true;
}

void Response::constructSentenceMappings(
    Response::SentenceMappings &sentenceMappings) {

  for (size_t i = 0; i < sourceRanges_.numSentences(); i++) {
    string_view src = sourceRanges_.sentence(i);
    string_view tgt = targetRanges_.sentence(i);
    sentenceMappings.emplace_back(src, tgt);
  }
}
} // namespace bergamot
} // namespace marian
