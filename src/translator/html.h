#ifndef SRC_BERGAMOT_HTML_H_
#define SRC_BERGAMOT_HTML_H_

#include <stdexcept>
#include <string>

namespace marian {
namespace bergamot {

class Response;

class BadHTML : public std::runtime_error {
 public:
  explicit BadHTML(const char *what) : std::runtime_error(what) {}
};

class HTML {
 public:
  explicit HTML(std::string &&source, bool process_markup);
  void Restore(Response &response);

 private:
  // TODO: store stuff here to be used for restoration.
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_HTML_H_
