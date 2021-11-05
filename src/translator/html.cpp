#include "html.h"

#include "response.h"
#include "xh_scanner.h"


namespace marian {
namespace bergamot {

HTML::HTML(std::string &&source, bool process_markup)
: original_(std::move(source)) {
  if (!process_markup) return;
  std::string original(std::move(source));
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
  AnnotatedText source(std::move(original_));

  auto span_it = spans_.begin();
  auto prev_it = spans_.end();
  std::size_t added_offset = 0;

  for (std::size_t sentenceIdx = 0; sentenceIdx < response.source.numSentences(); ++sentenceIdx) {
    std::vector<string_view> sentence;
    sentence.reserve(response.source.numWords(sentenceIdx));

    for (std::size_t wordIdx = 0; wordIdx < response.source.numWords(sentenceIdx); ++wordIdx) {
      ByteRange word = response.source.wordAsByteRange(sentenceIdx, wordIdx);
      ByteRange out{word.begin + added_offset, word.end + added_offset};
      
      while (
        span_it != spans_.end()
        && span_it->second.begin >= word.begin
        && span_it->second.begin < word.end
      ) {
        std:size_t added_html_size = span_it->first.begin - (span_it == spans_.begin() ? 0 : prev_it->first.end);
        out.end += added_html_size;
        added_offset += added_html_size;
        prev_it = span_it++;
      }

      sentence.emplace_back(source.text.data() + out.begin, out.end - out.begin);
    }

    // If there's additional HTML at the end, add it to the back of the last token
    // of the last sentence.
    if (sentenceIdx == response.source.numSentences() - 1 && span_it != spans_.end()) {
      std::size_t additional_html = span_it->first.begin - (prev_it == spans_.end() ? 0 : prev_it->first.end);
      sentence.back() = string_view(sentence.back().data(), sentence.back().size() + additional_html);
    }

    source.recordExistingSentence(sentence.begin(), sentence.end(), sentence[0].data());
  }

  response.source = source;
}

}  // namespace bergamot
}  // namespace marian
