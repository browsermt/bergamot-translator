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
  explicit BadHTML(std::string const &what) : std::runtime_error(what) {}
};

class HTML {
 public:
  struct Tag {
    std::string name;
    std::string attributes;
    bool empty;
  };

  typedef std::vector<Tag *> Taint;

  struct Span {
    size_t begin;
    size_t end;
    Taint tags;  // Note: free pointer! Lifetime of tags is managed by pool_
    inline size_t size() const { return end - begin; }
  };

  explicit HTML(std::string &&source, bool process_markup);
  void restore(Response &response);

 private:
  // List of text spans, and which tags are applied to them
  std::vector<Span> spans_;

  // a pool of tags that we free when HTML goes out of scope
  std::vector<std::unique_ptr<Tag>> pool_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_HTML_H_
