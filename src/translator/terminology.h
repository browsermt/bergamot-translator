#ifndef SRC_BERGAMOT_TERMINOLOGY_H_
#define SRC_BERGAMOT_TERMINOLOGY_H_

#include <string>
#include <unordered_map>

namespace marian {
namespace bergamot {

/// Map used for storing terminology.
using TerminologyMap = std::unordered_map<std::string, std::string>;

std::string ReplaceTerminology(std::string const &str, TerminologyMap const &terminology);

}  // namespace bergamot
}  // namespace marian

#endif