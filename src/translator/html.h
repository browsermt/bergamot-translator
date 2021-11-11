#ifndef SRC_BERGAMOT_HTML_H_
#define SRC_BERGAMOT_HTML_H_

#include <stdexcept>
#include <string>

#include "definitions.h"

namespace marian {
namespace bergamot {

struct Response;

class BadHTML : public std::runtime_error {
 public:
  explicit BadHTML(const char *what) : std::runtime_error(what) {}
};

class HTML {
 public:
  explicit HTML(std::string &&source, bool process_markup);
  void Restore(Response &response);

  // TODO Currently exposed just for tests
  inline const std::vector<std::pair<ByteRange, ByteRange>> spans() const { return spans_; };

 private:
  std::vector<std::pair<ByteRange, ByteRange>> spans_;
  std::string original_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_HTML_H_
