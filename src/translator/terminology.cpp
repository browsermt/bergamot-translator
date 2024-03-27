#include "terminology.h"

#include <sstream>

namespace marian {
namespace bergamot {

namespace {

struct Hit {
  size_t pos;
  TerminologyMap::const_iterator entry;
};

}  // namespace

std::string ReplaceTerminology(std::string const &str, TerminologyMap const &terminology) {
  size_t offset = 0;

  std::stringstream out;

  while (offset < str.size()) {
    Hit best{str.size()};

    for (auto it = terminology.begin(); it != terminology.end(); ++it) {
      // Search for the key `first` starting from our current search position
      auto pos = str.find(it->first, offset);

      if (pos == std::string::npos) continue;

      // Prefer earlier occurrences, and longer keys.
      if (pos < best.pos || pos == best.pos && it->first.size() > best.entry->first.size()) best = Hit{pos, it};
    }

    out << str.substr(offset, best.pos - offset);

    // best not default-initialised?
    if (best.pos < str.size()) {
      out << best.entry->second;
      offset = best.pos + best.entry->first.size();
    } else {
      offset = best.pos;  // will push it over str.size()
    }
  }

  return out.str();
}

}  // namespace bergamot
}  // namespace marian
