#include "translation_result.h"
#include "common/logging.h"
#include "data/alignment.h"

#include <utility>

namespace marian {
namespace bergamot {

TranslationResult::TranslationResult(std::string &&source,
                                     std::vector<TokenRanges> &&sourceRanges,
                                     Histories &&histories,
                                     std::vector<Ptr<Vocab const>> &vocabs)
    : source_(std::move(source)), sourceRanges_(std::move(sourceRanges)),
      histories_(std::move(histories)) {

  std::vector<string_view> sourceMappings;
  std::vector<string_view> targetMappings;

  // Process sourceMappings into sourceMappings.
  sourceMappings.reserve(sourceRanges_.size());
  for (int i = 0; i < sourceRanges_.size(); i++) {
    string_view first = sourceRanges_[i].front();
    string_view last = sourceRanges_[i].back();
    sourceMappings.emplace_back(first.data(), last.end() - first.begin());
  }

  // Compiles translations into a single std::string translation_
  // Current implementation uses += on std::string, multiple resizes.
  // Stores ByteRanges as indices first, followed by conversion into
  // string_views.
  // TODO(jerin): Add token level string_views here as well.
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

  // Converting ByteRanges as indices into string_views.
  targetMappings.reserve(translationRanges.size());
  for (auto &range : translationRanges) {
    const char *begin = &translation_[range.first];
    targetMappings.emplace_back(begin, range.second);
  }

  // Surely, let's add sentenceMappings_
  for (auto src = sourceMappings.begin(), tgt = targetMappings.begin();
       src != sourceMappings.end() && tgt != targetMappings.end();
       ++src, ++tgt) {
    sentenceMappings_.emplace_back(*src, *tgt);
    auto &t = sentenceMappings_.back();
  }
}

} // namespace bergamot
} // namespace marian
