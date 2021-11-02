#include "html.h"

#include "response.h"
#include "xh_scanner.h"

namespace marian {
namespace bergamot {

HTML::HTML(std::string &&source, bool process_markup) {
  if (!process_markup) return;
  std::string original(std::move(source));
  markup::instream in(original.data(), original.data() + original.size());
  markup::scanner scanner(in);
  source.clear();
  while (true) {
    switch (scanner.get_token()) {
      case markup::scanner::TT_ERROR:
        throw BadHTML("HTML parse error");
      case markup::scanner::TT_EOF:
        return;
      case markup::scanner::TT_TEXT:
        // Note these are byte offsets in the original input can can be used to adjust values.
        source.append(scanner.get_text_begin(), scanner.get_text_end());
        break;
        // TODO convert space-like tags such as <br/> and <img> to space.
      default:
        break;
    }
  }
  // TODO unescape entities.  See warc2text
}

void HTML::Restore(Response &response) {}

}  // namespace bergamot
}  // namespace marian
