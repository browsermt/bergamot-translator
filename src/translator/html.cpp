#include "html.h"

#include "response.h"
#include "xh_scanner.h"

namespace {
  void EncodeEntities(marian::string_view const &input, std::string &output) {
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
      RestoreTarget(response);
    RestoreSource(response);
  }
}

void HTML::RestoreSource(Response &response) {
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
      
      // While there is overlap with a span of text from the original HTML, there
      // might be HTML that we'll need to account for in our Annotation ranges.
      while (
        span_it != spans_.end()
        && span_it->second.begin >= word.begin
        && span_it->second.begin < word.end
      ) {
        // Slice in the HTML that comes before this text segment
        std:size_t added_html_size = span_it->first.begin - (span_it == spans_.begin() ? 0 : prev_it->first.end);
        
        // If this text segment is HTML with entities, the HTML will be longer
        // than the text version.
        assert(span_it->first.size() >= span_it->second.size()); // HTML is always longer than text
        added_html_size += span_it->first.size() - span_it->second.size();
          
        // Extend this word byterange to include all HTML that goes before it
        // and inside it (&..; entities, that is)
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
      assert(++span_it == spans_.end());
    }

    source.recordExistingSentence(sentence.begin(), sentence.end(), sentence[0].data());
  }

  response.source = source;
}

void HTML::RestoreTarget(Response &response) {
  AnnotatedText target;
  std::string html;

  double ratio = (double) response.target.text.size() / response.source.text.size();

  auto span_it = spans_.begin();
  auto prev_it = spans_.end();
  std::size_t added_offset = 0;
  
  for (std::size_t sentenceIdx = 0; sentenceIdx < response.target.numSentences(); ++sentenceIdx) {
    std::string sentence;
    std::vector<ByteRange> tokens;
    std::size_t added_html_size = 0;
    std::size_t sentence_offset = response.target.sentenceAsByteRange(sentenceIdx).begin;

    for (std::size_t wordIdx = 0; wordIdx < response.target.numWords(sentenceIdx); ++wordIdx) {
      ByteRange word = response.target.wordAsByteRange(sentenceIdx, wordIdx);
      ByteRange token{
        word.begin - sentence_offset + added_html_size,
        word.end   - sentence_offset + added_html_size
      };

      // Do encoding of any entities that popped up in the translation
      EncodeEntities(response.target.word(sentenceIdx, wordIdx), html);
      if (html.size() > word.size()) {
        token.end += html.size() - word.size();
        added_html_size += html.size() - word.size();
      }

      std::size_t html_prefix_size = 0; // bytes of html added to the beginning of this token.

      while (
        span_it != spans_.end()
        && span_it->second.begin >= static_cast<std::size_t>(ratio * word.begin)
        && span_it->second.begin < static_cast<std::size_t>(ratio * word.end)
      ) {
        // Slice in the HTML that comes before this text segment, but do make
        // sure to insert it after any previously inserted HTML as to maintain
        // the order the HTML occurred in in the input.
        std:size_t html_size = span_it->first.begin - (span_it == spans_.begin() ? 0 : prev_it->first.end);
        html.insert(html_prefix_size, original_, span_it == spans_.begin() ? 0 : prev_it->first.end, html_size);
        html_prefix_size += html_size;
        
        prev_it = span_it++;
      }

      token.end += html_prefix_size;
      added_html_size += html_prefix_size;

      sentence.append(html);
      tokens.push_back(token);
    }

    // TODO: append ending whitespace?

    // If there's additional HTML at the end, add it to the back of the last token
    // of the last sentence.
    if (sentenceIdx == response.source.numSentences() - 1 && span_it != spans_.end()) {
      std::size_t html_size = span_it->first.begin - (span_it == spans_.begin() ? 0 : prev_it->first.end);
      sentence.append(original_, span_it == spans_.begin() ? 0 : prev_it->first.end, html_size);
      tokens.back().end += html_size;
      assert(++span_it == spans_.end());
    }

    std::vector<string_view> token_views;
    token_views.reserve(tokens.size());
    
    for (auto token : tokens)
      token_views.emplace_back(sentence.data() + token.begin, token.size());

    target.appendSentence(string_view(), token_views.begin(), token_views.end());
  }

  response.target = target;
}

}  // namespace bergamot
}  // namespace marian
