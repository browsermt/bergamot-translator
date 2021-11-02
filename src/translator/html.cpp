#include "html.h"
#include "xh_scanner.h"

namespace marian {
namespace bergamot {

bool HTML::Strip(std::string &&source) {
  std::string original(std::move(source));
  markup::instream in(original.data(), original.data() + original.size());
  markup::scanner scanner(in);
  source.clear();
  while (true) {
    switch (scanner.get_token()) {
      case markup::scanner::TT_ERROR:
        return false;
      case markup::scanner::TT_EOF:
        return true;
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

} // namespace bergamot
} // namespace marian
