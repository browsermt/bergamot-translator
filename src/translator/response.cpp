#include "response.h"
#include "common/logging.h"
#include "data/alignment.h"

#include <utility>

namespace marian {
namespace bergamot {

Response::Response(std::string &&source,
                   std::vector<TokenRanges> &&sourceRanges,
                   Histories &&histories, std::vector<Ptr<Vocab const>> &vocabs)
    : source_(std::move(source)), sourceRanges_(std::move(sourceRanges)),
      histories_(std::move(histories)) {

  constructTargetProperties(vocabs);
}

void Response::move(std::string &source, std::string &translation,
                    SentenceMappings &sentenceMappings) {

  constructSentenceMappings(sentenceMappings);
  // Totally illegal stuff.
  source = std::move(source_);
  translation = std::move(translation_);

  // The above assignment expects source, target be moved.
  // which makes the following invalid, hence required to be cleared.
  sourceRanges_.clear();
  targetRanges_.clear();
  histories_.clear();
}

void Response::constructTargetProperties(
    std::vector<Ptr<Vocab const>> &vocabs) {
  std::vector<std::pair<int, int>> translationRanges;
  size_t offset{0};
  bool first{true};
  for (auto &history : histories_) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0]; // Expecting only one result;
    Words words = std::get<0>(result);
    std::string decoded = (vocabs.back())->decode(words);
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

  // TODO(@jerinphilip):
  // Currently considers target tokens as whole text. Needs
  // to be further enhanced in marian-dev to extract alignments.
  for (auto &range : translationRanges) {
    std::vector<string_view> targetMappings;
    const char *begin = &translation_[range.first];
    targetMappings.emplace_back(begin, range.second);
    targetRanges_.push_back(std::move(targetMappings));
  }
}

void Response::constructSentenceMappings(
    Response::SentenceMappings &sentenceMappings) {

  for (int i = 0; i < sourceRanges_.size(); i++) {
    string_view first, last;

    // Handle source-sentence
    first = sourceRanges_[i].front();
    last = sourceRanges_[i].back();
    string_view src_sentence(first.data(), last.end() - first.begin());

    // Handle target-sentence
    first = targetRanges_[i].front();
    last = targetRanges_[i].back();
    string_view tgt_sentence(first.data(), last.end() - first.begin());

    // Add both into sentence-mappings
    sentenceMappings.emplace_back(src_sentence, tgt_sentence);
  }
}
} // namespace bergamot
} // namespace marian
