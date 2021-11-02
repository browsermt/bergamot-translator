#ifndef SRC_BERGAMOT_HTML_H_
#define SRC_BERGAMOT_HTML_H_

#include <string>

namespace marian {
namespace bergamot {

// TODO plan is have an object that stores tag information in the internal Request object, then restores it later
class HTML {
 public:
  // Returns true on successful parse
  static bool Strip(std::string &&source);
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_HTML_H_
