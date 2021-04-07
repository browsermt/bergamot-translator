#pragma once
#include "data/types.h"
#include "definitions.h"
#include "sentence_ranges.h"
#include "text_processor.h"

#include "common/options.h"
#include "data/vocab.h"
#include <vector>

namespace marian {
namespace bergamot {

inline void debugContiguity(const std::string tag, const string_view &segment,
                            std::vector<string_view> &wordRanges) {
  std::vector<std::pair<size_t, size_t>> faults;
  for (size_t i = 1; i < wordRanges.size(); i++) {
    string_view first = wordRanges[i - 1];
    string_view second = wordRanges[i];
    bool assertion = (first.data() + first.size()) == second.data();
    if (assertion == false) {
      faults.emplace_back(i - 1, i);
    }
    // ABORT_IF(assertion == false,
    //          "Something's wrong, we don't have consecutive words");
  }
  if (faults.empty()) {
    // LOG(info, "All okay at line: {}", std::string(segment));
  } else {
    LOG(info, " [fail] Something's wrong in {} line(length={}): {}", tag,
        segment.size(), std::string(segment));
    std::stringstream faultsStream;
    bool first = true;
    for (auto &p : faults) {
      if (not first) {
        first = false;
      } else {
        faultsStream << " ";
      }
      size_t i = p.first, j = p.second;

      auto rebase = [&segment](const char *data) {
        size_t diff = data - segment.data();
        return diff;
      };

      faultsStream << "(" << rebase(wordRanges[i].data()) << ","
                   << rebase(wordRanges[i].data() + wordRanges[i].size())
                   << ")";
      faultsStream << "-" << rebase(wordRanges[j].data());
    }
    LOG(info, "At: {} ", faultsStream.str());
  }
}

} // namespace bergamot
} // namespace marian
