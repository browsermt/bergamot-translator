// https://www.codeproject.com/Articles/14076/Fast-and-Compact-HTML-XML-Scanner-Tokenizer
// BSD license

#include "xh_scanner.h"

#include <cassert>
#include <cctype>
#include <cstring>

namespace {

// Simple replacement for string_view.ends_with(compile-time C string)
template <typename Char_t, size_t Len>
inline bool endsWith(markup::string_ref &str, const Char_t (&suffix)[Len]) {
  size_t offset = str.size - (Len - 1);
  return offset <= str.size && std::memcmp(str.data + offset, suffix, Len - 1) == 0;
}

inline bool equalsCaseInsensitive(const char *lhs, const char *rhs, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    // cast to unsigned char otherwise std::tolower has undefined behaviour
    if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i])))
      return false;
  }

  return true;
}

// Alias for the above, but with compile-time known C string
template <size_t Len>
inline bool equalsCaseInsensitive(markup::string_ref &lhs, const char (&rhs)[Len]) {
  return lhs.size == Len - 1 && equalsCaseInsensitive(lhs.data, rhs, Len - 1);
}

template <typename Char_t, size_t Len>
bool operator==(markup::string_ref const &str, const Char_t (&str2)[Len]) {
  return str.size == Len - 1 && std::memcmp(str.data, str2, Len - 1) == 0;
}

template <size_t N>
constexpr size_t length(char const (&/*unused*/)[N]) {
  return N - 1;
}

}  // end namespace

namespace markup {

// case sensitive string equality test
// s_lowcase shall be lowercase string
std::string_view Scanner::value() const { return std::string_view(value_.data, value_.size); }

std::string_view Scanner::attribute() const { return std::string_view(attributeName_.data, attributeName_.size); }

std::string_view Scanner::tag() const { return std::string_view(tagName_.data, tagName_.size); }

Scanner::TokenType Scanner::scanBody() {
  value_ = string_ref{input_.pos(), 0};

  start_ = input_.pos();

  switch (input_.peek()) {
    case '\0':
      return TT_EOF;
    case '<':
      return scanTag();
    case '&':
      return scanEntity(TT_TEXT);
  }

  while (true) {
    switch (input_.peek()) {
      case '\0':
      case '<':
      case '&':
        return TT_TEXT;
      default:
        input_.consume();
        ++value_.size;
        break;
    }
  }
}

// Consumes one or closing bit of a tag:
//   <tag attr="value">...</tag>
//       |------------|
// Followed by:
// - scanSpecial if <script> or <style>
// - scanBody
// - another scan_head for the next attribute or end of open tag
// Returns:
// - TT_ATTRIBUTE if attribute is read
// - TT_TAG_END if self-closing tag
// - TT_ERROR if wrong character encountered
// - TT_EOF if unexpected end of input (will not return TT_ATTRIBUTE if attribute value wasn't finished yet)
// - TT_TAG_END through scanSpecial
// - TT_TEXT through scanBody
Scanner::TokenType Scanner::scanAttribute() {
  // Skip all whitespace between tag name or last attribute and next attribute or '>'
  skipWhitespace();

  // Find end of tag name
  switch (input_.peek()) {
    case '>':
      input_.consume();

      // Treat some elements as opaque, e.g. <script>, <style>
      if (/*equalsCaseInsensitive(tagName_, "title") ||*/ equalsCaseInsensitive(tagName_, "script") ||
          equalsCaseInsensitive(tagName_, "style") || equalsCaseInsensitive(tagName_, "textarea") ||
          equalsCaseInsensitive(tagName_, "iframe") || equalsCaseInsensitive(tagName_, "noembed") ||
          equalsCaseInsensitive(tagName_, "noscript") || equalsCaseInsensitive(tagName_, "noframes")) {
        // script is special because we want to parse the attributes,
        // but not the content
        scanFun_ = &Scanner::scanSpecial;
        return scanSpecial();
      } else {
        scanFun_ = &Scanner::scanBody;
        return scanBody();
      }
    case '/':
      input_.consume();
      if (input_.peek() == '>') {
        // self closing tag
        input_.consume();
        scanFun_ = &Scanner::scanBody;
        return TT_TAG_END;
      } else {
        return TT_ERROR;
      }
  }

  attributeName_ = string_ref{input_.pos(), 0};
  value_ = string_ref{nullptr, 0};

  // attribute name...
  while (input_.peek() != '=') {
    switch (input_.peek()) {
      case '\0':
        return TT_EOF;
      case '>':
        return TT_ATTRIBUTE;  // attribute without value (HTML style) at end of tag
      case '<':
        return TT_ERROR;
      default:
        if (skipWhitespace()) {
          if (input_.peek() == '=') {
            break;
          } else {
            return TT_ATTRIBUTE;  // attribute without value (HTML style) but not yet at end of tag
          }
        }
        input_.consume();
        ++attributeName_.size;
        break;
    }
  }

  // consume '=' and any following whitespace
  input_.consume();
  skipWhitespace();
  // attribute value...

  char quote;  // Either '"' or '\'' depending on which quote we're searching for
  switch (input_.peek()) {
    case '"':
    case '\'':
      quote = input_.consume();
      value_ = string_ref{input_.pos(), 0};
      while (true) {
        if (input_.peek() == '\0') {
          return TT_ERROR;
        } else if (input_.peek() == quote) {
          input_.consume();
          return TT_ATTRIBUTE;
        } else {
          input_.consume();
          ++value_.size;
        }
      }
      break;
    default:
      value_ = string_ref{input_.pos(), 0};

      while (true) {
        if (isWhitespace(input_.peek())) return TT_ATTRIBUTE;
        if (input_.peek() == '>') return TT_ATTRIBUTE;  // '>' will be consumed next round
        input_.consume();
        ++value_.size;
      }
      break;
  }

  // How did we end up here?!
  return TT_ERROR;
}

// scans tag name of open or closing tag
//   <tag attr="value">...</tag>
//   |--|                 |----|
// Emits:
// - TT_TAG_START if tag head is read
// - TT_COMMENT_START
// - TT_PROCESSING_INSTRUCTION_START
// - TT_CDATA_START
// - TT_ENTITY_START
// - TT_ERROR if unexpected character or end
Scanner::TokenType Scanner::scanTag() {
  start_ = input_.pos();
  if (input_.consume() != '<') return TT_ERROR;

  bool isTail = input_.peek() == '/';
  if (isTail) input_.consume();

  tagName_ = string_ref{input_.pos(), 0};

  while (input_.peek()) {
    if (skipWhitespace()) break;

    if (input_.peek() == '/' || input_.peek() == '>') break;

    input_.consume();
    ++tagName_.size;

    // Note: these tests are executed at every char, thus eager.
    // "<?xml" will match on `tagName_ == "?"`.
    if (tagName_ == "!--") {
      scanFun_ = &Scanner::scanComment;
      return TT_COMMENT_START;
    } else if (tagName_ == "?") {
      scanFun_ = &Scanner::scanProcessingInstruction;
      return TT_PROCESSING_INSTRUCTION_START;
    }
  }

  if (!input_.peek()) return TT_EOF;

  if (isTail) return input_.consume() == '>' ? TT_TAG_END : TT_ERROR;

  scanFun_ = &Scanner::scanAttribute;
  return TT_TAG_START;
}

Scanner::TokenType Scanner::scanEntity(TokenType parentTokenType) {
  // `entity` includes starting '&' and ending ';'
  start_ = input_.pos();
  string_ref entity{input_.pos(), 0};
  bool hasEnd = false;

  if (input_.consume() != '&') return TT_ERROR;

  ++entity.size;  // Account for the consumed '&'

  // Consume the entity
  while (input_.peek()) {
    if (input_.peek() == ';') {
      input_.consume();
      ++entity.size;
      hasEnd = true;
      break;
    } else if (!isalpha(input_.peek())) {
      hasEnd = false;
      break;
    } else {
      input_.consume();
      ++entity.size;
    }
  }

  // If we can decode the entity, do so
  if (hasEnd && resolveEntity(entity, value_)) return parentTokenType;

  // Otherwise, just yield the whole thing undecoded, interpret it as text
  value_ = entity;
  return parentTokenType;
}

bool Scanner::resolveEntity(string_ref const &buffer, string_ref &decoded) const {
  static char lt = '<', gt = '>', amp = '&', quot = '"', apos = '\'', nbsp = ' ';

  if (buffer == "&lt;") {
    decoded = string_ref{&lt, 1};
    return true;
  }
  if (buffer == "&gt;") {
    decoded = string_ref{&gt, 1};
    return true;
  }
  if (buffer == "&amp;") {
    decoded = string_ref{&amp, 1};
    return true;
  }
  if (buffer == "&quot;") {
    decoded = string_ref{&quot, 1};
    return true;
  }
  if (buffer == "&apos;") {
    decoded = string_ref{&apos, 1};
    return true;
  }
  if (buffer == "&nbsp;") {
    decoded = string_ref{&nbsp, 1};  // TODO: handle non-breaking spaces better than just converting them to spaces
    return true;
  }
  return false;
}

// skip whitespaces.
// returns how many whitespaces were skipped
size_t Scanner::skipWhitespace() {
  size_t skipped = 0;
  while (isWhitespace(input_.peek())) {
    input_.consume();
    ++skipped;
  }
  return skipped;
}

bool Scanner::isWhitespace(char c) {
  return c <= ' ' && (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f');
}

Scanner::TokenType Scanner::scanComment() {
  if (gotTail_) {
    start_ = input_.pos() - length("-->");  // minus "-->"
    scanFun_ = &Scanner::scanBody;
    gotTail_ = false;
    return TT_COMMENT_END;
  }

  start_ = input_.pos();
  value_ = string_ref{input_.pos(), 0};

  while (true) {
    if (input_.consume() == '\0') return TT_EOF;
    ++value_.size;

    if (endsWith(value_, "-->")) {
      gotTail_ = true;
      value_.size -= length("-->");
      break;
    }
  }
  return TT_DATA;
}

Scanner::TokenType Scanner::scanProcessingInstruction() {
  if (gotTail_) {
    start_ = input_.pos() - length("?>");
    scanFun_ = &Scanner::scanBody;
    gotTail_ = false;
    return TT_PROCESSING_INSTRUCTION_END;
  }

  start_ = input_.pos();
  value_ = string_ref{input_.pos(), 0};

  while (true) {
    if (input_.consume() == '\0') return TT_EOF;
    ++value_.size;

    if (endsWith(value_, "?>")) {
      gotTail_ = true;
      value_.size -= length("?>");
      break;
    }
  }
  return TT_DATA;
}

Scanner::TokenType Scanner::scanSpecial() {
  if (gotTail_) {
    start_ = input_.pos() - (tagName_.size + length("</>"));
    scanFun_ = &Scanner::scanBody;
    gotTail_ = false;
    return TT_TAG_END;
  }

  start_ = input_.pos();
  value_ = string_ref{input_.pos(), 0};

  while (true) {
    if (input_.consume() == '\0') return TT_EOF;
    ++value_.size;

    // Test for </tag>
    // TODO: no whitespaces allowed? Is that okay?
    if (value_.data[value_.size - 1] == '>' && value_.size >= tagName_.size + length("</>")) {
      // Test for the "</"" bit of "</tag>"
      size_t posTagStart = value_.size - tagName_.size - length("</>");
      if (std::memcmp(value_.data + posTagStart, "</", length("</")) != 0) continue;

      // Test for the "tag" bit of "</tag>". Doing case insensitive compare because <I>...</i> is okay.
      size_t posTagName = value_.size - tagName_.size - length(">");  // end - tag>
      if (!equalsCaseInsensitive(value_.data + posTagName, tagName_.data, tagName_.size)) continue;

      gotTail_ = true;
      value_.size -= tagName_.size + length("</>");
      break;
    }
  }

  return TT_DATA;
}

}  // namespace markup
