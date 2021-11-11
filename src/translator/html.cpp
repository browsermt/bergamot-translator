#include "html.h"

#include "response.h"
#include "xh_scanner.h"

namespace {
  using marian::bergamot::ByteRange;
  using marian::bergamot::AnnotatedText;
  using marian::string_view;

  void EncodeEntities(string_view const &input, std::string &output) {
    output.clear();
    output.reserve(input.size());

    for (auto it = input.begin(); it != input.end(); ++it) {
      switch (*it) {
        case '&':
          output.append("&amp;");
          break;
        case '<':
          output.append("&lt;");
          break;
        case '>':
          output.append("&gt;");
          break;
        // case '"':
        //   output.append("&quot;");
        //   break;
        // case '\'':
        //   output.append("&apos;");
        //   break;
        default:
          output.push_back(*it);
          break;
      }
    }
  }

  void Reconstruct(std::vector<std::pair<ByteRange,ByteRange>> const &spans, std::string const &original, AnnotatedText &text, double ratio) {
    auto span_it = spans.begin();
    auto prev_it = spans.end();

    std::string html; // workspace

    auto inRange = [&](ByteRange range) {
      return span_it->second.begin >= static_cast<std::size_t>(ratio * range.begin)
          && span_it->second.begin < static_cast<std::size_t>(ratio * range.end);
    };
    
    text = text.apply([&](ByteRange range, string_view token, bool last) {
      // Do encoding of any entities that popped up in the translation
      EncodeEntities(token, html);
      
      std::size_t html_prefix_size = 0; // bytes of html added to the beginning of this token.

      while (span_it != spans.end() && (last || inRange(range))) {
        // Slice in the HTML that comes before this text segment, but do make
        // sure to insert it after any previously inserted HTML as to maintain
        // the order the HTML occurred in in the input.
        std:size_t html_size = span_it->first.begin - (span_it == spans.begin() ? 0 : prev_it->first.end);
        html.insert(html_prefix_size, original, span_it == spans.begin() ? 0 : prev_it->first.end, html_size);
        html_prefix_size += html_size;

        prev_it = span_it++;
      }

      return html;
    });

    assert(span_it == spans.end());
  }
}

namespace marian {
namespace bergamot {

HTML::HTML(std::string &&source, bool process_markup)
: original_(std::move(source)) {
  if (!process_markup) return;
  markup::instream in(original_.data(), original_.data() + original_.size());
  markup::scanner scanner(in);
  source.clear();
  bool stop = false;
  while (!stop) {
    auto begin = source.size();
    switch (scanner.get_token()) {
      case markup::scanner::TT_ERROR:
        throw BadHTML("HTML parse error");
      case markup::scanner::TT_EOF:
        stop = true;
        break;
      case markup::scanner::TT_TEXT:
        source.append(scanner.get_value());
        spans_.emplace_back(
          ByteRange{
            static_cast<std::size_t>(scanner.get_text_begin() - original_.data()),
            static_cast<std::size_t>(scanner.get_text_end() - original_.data())
          },
          ByteRange{
            begin,
            source.size()
          });
        break;
        // TODO convert space-like tags such as <br/> and <img> to space.
      default:
        break;
    }
  }
  // TODO unescape entities.  See warc2text
}


void HTML::Restore(Response &response) {
  if (!response.source.text.empty()) {
    if (!response.target.text.empty())
      ::Reconstruct(spans_, original_, response.target, (double) response.target.text.size() / response.source.text.size());
    ::Reconstruct(spans_, original_, response.source, 1.0);
  }
}

}  // namespace bergamot
}  // namespace marian
