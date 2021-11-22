#include "html.h"

#include "response.h"
#include "xh_scanner.h"

namespace {
using marian::string_view;
using marian::bergamot::AnnotatedText;
using marian::bergamot::ByteRange;
using marian::bergamot::HTML;

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

size_t CountPrefixWhitespaces(string_view const &input) {
  size_t size = 0;
  while (size < input.size() && input[size] == ' ') ++size;
  return size;
}

std::ostream &operator<<(std::ostream &out, HTML::Tag const *tag) { return out << '<' << tag->name << '>'; }

std::ostream &operator<<(std::ostream &out, HTML::Taint const &tags) {
  for (auto it = tags.begin(); it != tags.end(); ++it) {
    if (it != tags.begin()) out << ' ';
    out << *it;
  }
  return out;
}

// Very simple replacement for std::format introduced in C++20
std::string format(std::string const &format_str) { return format_str; }

template <typename Arg>
std::string format(std::string const &format_str, Arg arg) {
  std::ostringstream os;
  auto index = format_str.find("{}");
  assert(index != std::string::npos);
  os << format_str.substr(0, index) << arg << format_str.substr(index + 2);
  return os.str();
}

template <typename Arg, typename... Args>
std::string format(std::string const &format_str, Arg arg, Args... args) {
  std::ostringstream os;
  auto index = format_str.find("{}");
  assert(index != std::string::npos);
  os << format_str.substr(0, index) << arg << format(format_str.substr(index + 2), std::forward<Args>(args)...);
  return os.str();
}

bool IsBlockElement(const char *name) {
  // List of elements that we expect might occur inside words, and that should
  // not introduce spacings around them. Not strictly inline elements, nor flow
  // elements. See also https://developer.mozilla.org/en-US/docs/Web/Guide/HTML/Content_categories
  static std::unordered_set<std::string> inline_ish_elements{
      "abbr",  "a",    "b",      "em",  "i",   "kbd",  "mark", "math", "output", "q",   "ruby",
      "small", "span", "strong", "sub", "sup", "time", "u",    "var",  "wbr",    "ins", "del"};

  return inline_ish_elements.find(name) == inline_ish_elements.end();
}

bool IsEmtpyElement(const char *name) {
  // List of elements for which we do not expect a closing tag, or self-closing
  // elements in XHTML. See also https://developer.mozilla.org/en-US/docs/Glossary/Empty_element
  static std::unordered_set<std::string> empty_elements{"area",  "base", "br",   "col",   "embed",  "hr",    "img",
                                                        "input", "link", "meta", "param", "source", "track", "wbr"};

  return empty_elements.find(name) != empty_elements.end();
}

void DiffTags(HTML::Taint const &prev, HTML::Taint const &curr, HTML::Taint &opening, HTML::Taint &closing) {
  opening.clear();
  closing.clear();

  size_t i = 0;

  // Find first difference
  for (; i < prev.size(); ++i)
    if (i >= curr.size() || prev[i] != curr[i]) break;

  std::copy_if(prev.begin() + i, prev.end(), std::back_inserter(closing), [&](HTML::Tag *tag) { return !tag->empty; });

  opening.insert(opening.end(), curr.begin() + i, curr.end());

  // std::cerr << "Comparing " << prev << " to " << curr
  //           << ": closing " << closing << ", opening " << opening
  //           << std::endl;
}

bool Intersects(ByteRange const &range, HTML::Span const &span) {
  return range.begin <= span.end && range.end >= span.begin;
};

void FilterEmpty(HTML::Taint &stack) {
  auto src = stack.begin();
  auto dst = stack.begin();

  for (auto src = stack.begin(); src != stack.end(); ++src)
    if (!(*src)->empty) *(dst++) = *src;

  stack.resize(dst - stack.begin());
}

template <typename Fun>
AnnotatedText Apply(AnnotatedText const &in, Fun fun) {
  AnnotatedText out;

  for (size_t sentenceIdx = 0; sentenceIdx < in.numSentences(); ++sentenceIdx) {
    std::string sentence;
    std::vector<ByteRange> tokens;

    std::string prefix = fun(in.annotation.gap(sentenceIdx),
                             in.gap(sentenceIdx),
                             false);

    for (size_t wordIdx = 0; wordIdx < in.numWords(sentenceIdx); ++wordIdx) {
      std::string token = fun(in.wordAsByteRange(sentenceIdx, wordIdx),
                              in.word(sentenceIdx, wordIdx),
                              false);
      tokens.push_back(ByteRange{sentence.size(), sentence.size() + token.size()});
      sentence += token;
    }

    // Convert our ByteRanges to string_views since that's what appendSentence
    // expects
    // TODO: extend AnnotatedText::appendSentence to accept str + ByteRanges
    // directly
    std::vector<string_view> token_views(tokens.size());
    std::transform(tokens.begin(), tokens.end(), token_views.begin(), [&](ByteRange const &range) {
      return string_view(sentence.data() + range.begin, range.size());
    });
    
    out.appendSentence(prefix, token_views.begin(), token_views.end());
  }

  out.appendEndingWhitespace(fun(in.annotation.gap(in.numSentences()),
                                 in.gap(in.numSentences()),
                                 true));

  std::cerr << "::: " << out.text << " :::\n";

  return out;
}

}  // namespace

namespace marian {
namespace bergamot {

HTML::HTML(std::string &&source, bool process_markup) {
  if (!process_markup) return;
  std::string original = std::move(source);
  markup::instream in(original.data(), original.data() + original.size());
  markup::scanner scanner(in);
  source.clear();  // source is moved out of, so should be clear anyway

  Taint stack;
  spans_.push_back(Span{0, 0, {}});

  bool stop = false;
  while (!stop) {
    switch (scanner.get_token()) {
      case markup::scanner::TT_ERROR:
        throw BadHTML("HTML parse error");

      case markup::scanner::TT_EOF:
        stop = true;
        break;

      case markup::scanner::TT_TEXT: {
        auto begin = source.size();
        source.append(scanner.get_value());
        spans_.push_back(Span{begin, source.size(), stack});
        FilterEmpty(stack);
      } break;

      case markup::scanner::TT_TAG_START:
        // If it makes sense to treat this element as a break in a word (e.g.
        // <br>, <img>, <li>) make sure it does so in this text as well.
        // TODO: Strong assumption here that the language uses spaces to
        // separate words
        if (IsBlockElement(scanner.get_tag_name()) && !source.empty() && source.back() != ' ') source.push_back(' ');

        pool_.emplace_back(new Tag{
            scanner.get_tag_name(), std::string(),
            IsEmtpyElement(scanner.get_tag_name())  // TODO: detect empty elements by doing a second pass and detecting
                                                    // non-closed elements?
        });

        stack.push_back(pool_.back().get());
        break;

      case markup::scanner::TT_TAG_END:
        // Note: self-closing tags emit TT_TAG_END immediately after TT_TAG_START
        // but since we're parsing HTML5, a sole <img> will never emit a TT_TAG_END
        if (stack.empty())
          throw BadHTML(format("Encountered more closing tags ({}) than opening tags", scanner.get_tag_name()));

        if (stack.back()->name != scanner.get_tag_name())
          throw BadHTML(format("Encountered unexpected closing tag <{}>, top of stack is <{}>", scanner.get_tag_name(),
                               stack.back()));

        // TODO: what to do with "<u></u>" case, where tag is immediately closed
        // so it never makes it into the taint of any of the spans? Add it as
        // an empty tag to the previous/following?
        stack.pop_back();
        break;

      case markup::scanner::TT_ATTR:
        // TODO could be more efficient if format() accepted a destination, i.e. format_to?
        stack.back()->attributes += format(" {}=\"{}\"", scanner.get_attr_name(), scanner.get_value());
        break;

      default:
        break;
    }
  }

  if (!stack.empty()) throw BadHTML(format("Not all tags were closed: {}", stack));

  // Add a trailing span (that's empty) to signify all closed tags.
  spans_.emplace_back(Span{source.size(), source.size(), stack});

  // Test case: Swap some spans, see whether we still end up with valid HTML
  // std::swap(spans_[0], spans_[4]);
  /*
  {
    std::cerr << "Input: " << original << "\n";
    std::cerr << "Reconstructed: ";

    auto it = spans_.begin();
    auto prev = spans_.end();

    for (;it != spans_.end(); prev = it++) {
      Taint opening;
      Taint closing;

      if (prev != spans_.end()) {
        size_t i = 0;

        // Find first difference
        for (; i < prev->tags.size(); ++i)
          if (i >= it->tags.size() || prev->tags[i] != it->tags[i])
            break;

        closing.insert(closing.end(), prev->tags.begin() + i, prev->tags.end());
        opening.insert(opening.end(), it->tags.begin() + i, it->tags.end());
      } else {
        opening.insert(opening.end(), it->tags.begin(), it->tags.end());
      }

      for (auto cit = closing.crbegin(); cit != closing.crend(); ++cit)
        std::cerr << "</" << (*cit)->name << ">";
      for (Tag const *tag : opening)
        std::cerr << "<" << tag->name << ">";
      std::cerr << source.substr(it->begin, it->size());
    }

    for (Tag *tag : spans_.back().tags)
      std::cerr << "</" << tag->name << ">";

    std::cerr << "\n";
  }
  */
}

void HTML::Restore(Response &response) {
  if (spans_.empty()) return;

  // Reconstruction of HTML tags:
  // 1. Map each token to a Span
  // 2. Apply the taint of that span to the token
  // 3. Reconstruct the source HTML with these tainted tokens
  // 4. Transfer the taint from the source tokens to the target tokens using alignment information
  // 5. Reconstruct the target HTML with these tainted tokens

  std::vector<Taint> token_tags;
  token_tags.emplace_back();  // Empty stack

  auto span_it = spans_.begin();
  auto prev_it = spans_.begin();  // using end() to indicate first token

  std::string html;  // workspace

  AnnotatedText source = Apply(response.source, [&](ByteRange range, string_view token, bool last) {
    Taint opening, closing;

    // Do encoding of any entities that popped up in the translation
    // (Also effectively clears html from previous call)
    EncodeEntities(token, html);

    size_t offset = 0;  // Size added by prepending HTML
    size_t whitespace_size = CountPrefixWhitespaces(token);

    // Potential issue: spans and tokens can intersect, e.g.
    //
    //    text  <p> h <u> e </u> ll o </p>
    //   spans     |1|   |2|    |3333| (so only 2 is tainted with <p><u>, others only <p>)
    //  tokens     |111111111111111|2|
    //
    // Now 1 covers span 1 to 3, so what taint should it get? Just <p>, or <p><u>?

    // Seek to the last span that overlaps with this token
    while (true) {
      DiffTags(prev_it->tags, span_it->tags, opening, closing);
      prev_it = span_it;

      for (auto cit = closing.crbegin(); cit != closing.crend(); ++cit) {
        std::string close_tag = format("</{}>", (*cit)->name);
        html.insert(offset, close_tag);
        offset += close_tag.size();
      }

      for (Tag const *tag : opening) {
        std::string open_tag = format("<{}{}>", tag->name, tag->attributes);
        html.insert(offset + whitespace_size, open_tag);
        offset += open_tag.size();
      }

      if (span_it + 1 != spans_.end() && (span_it + 1)->begin < range.end) {
        span_it++;
        continue;
      }

      break;
    }

    // TODO: This is just the taint of the last span, not the ones in between
    // I don't know if that is okay for transferring taints. We'll need to test.
    token_tags.push_back(prev_it->tags);

    return html;
  });


  auto token_prev_it = token_tags_target.begin();
  auto token_tags_it = token_tags_target.begin() + 1;

  AnnotatedText target = Apply(response.target, [&](ByteRange range, string_view token, bool last) {
    Taint opening, closing;
    
    // Do encoding of any entities that popped up in the translation
    // (Also effectively clears html from previous call)
    EncodeEntities(token, html);

    size_t offset = 0;  // Size added by prepending HTML
    size_t whitespace_size = CountPrefixWhitespaces(token);

    assert(token_tags_it != token_tags_target.end());
    DiffTags(*token_prev_it, *token_tags_it, opening, closing);

    for (auto cit = closing.crbegin(); cit != closing.crend(); ++cit) {
      std::string close_tag = format("</{}>", (*cit)->name);
      html.insert(offset, close_tag);
      offset += close_tag.size();
    }

    for (Tag const *tag : opening) {
      std::string open_tag = format("<{}{}>", tag->name, tag->attributes);
      html.insert(offset + whitespace_size, open_tag);
      offset += open_tag.size();
    }

    // If this is the last token of the response, close all open tags.
    if (last) {
      for (auto cit = token_tags_it->crbegin(); cit != token_tags_it->crend(); ++cit) {
        html += format("</{}>", (*cit)->name);
      }
    }

    ++token_prev_it;
    ++token_tags_it;

    return html;
  });

  assert(token_tags_it == token_tags_target.end());

  response.source = source;
  response.target = target;
}

}  // namespace bergamot
}  // namespace marian
