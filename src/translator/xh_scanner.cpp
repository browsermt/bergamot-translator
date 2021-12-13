// https://www.codeproject.com/Articles/14076/Fast-and-Compact-HTML-XML-Scanner-Tokenizer
// BSD license

#include "xh_scanner.h"

#include <cctype>
#include <cstring>
#include <iostream>

namespace {

// Simple replacement for str.ends_with(compile-time C string)
template <typename Char_t, size_t Len>
inline bool ends_with(std::string const &str, const Char_t (&suffix)[Len]) {
  size_t offset;
  return __builtin_sub_overflow(str.size(), Len - 1, &offset) == 0 &&
         std::memcmp(str.data() + offset, suffix, Len - 1) == 0;
}

inline bool equals_case_insensitive(const char *lhs, const char *rhs, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    // cast to unsigned char otherwise std::tolower has undefined behaviour
    if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i])))
      return false;
  }

  return true;
}

// Alias for the above, but with compile-time known C string
template <size_t Len>
inline bool equals_case_insensitive(std::string const &lhs, const char (&rhs)[Len]) {
  return lhs.size() == Len - 1 && equals_case_insensitive(lhs.data(), rhs, Len);
}

}  // end namespace

namespace markup {

// case sensitive string equality test
// s_lowcase shall be lowercase string
const char *scanner::get_value() { return value_.data(); }

const char *scanner::get_attr_name() { return attr_name_.data(); }

const char *scanner::get_tag_name() { return tag_name_.data(); }

scanner::token_type scanner::scan_body() {
  text_begin = input.p;
  if (input_char) {
    --text_begin;
  }
  text_end = text_begin;
  value_.clear();
  char c = get_char();

  if (c == 0)
    return TT_EOF;
  else if (c == '<')
    return scan_tag();
  else if (c == '&')
    return scan_entity();

  while (true) {
    value_.push_back(c);
    ++text_end;

    c = get_char();

    if (c == 0) {
      push_back(c);
      break;
    }
    if (c == '<') {
      push_back(c);
      break;
    }
    if (c == '&') {
      push_back(c);
      break;
    }
  }
  return TT_TEXT;
}

scanner::token_type scanner::scan_head() {
  char c = skip_whitespace();

  if (c == '>') {
    if (equals_case_insensitive(tag_name_, "script")) {
      // script is special because we want to parse the attributes,
      // but not the content
      c_scan = &scanner::scan_special;
      return scan_special();
    } else if (equals_case_insensitive(tag_name_, "style")) {
      // same with style
      c_scan = &scanner::scan_special;
      return scan_special();
    }
    c_scan = &scanner::scan_body;
    return scan_body();
  }
  if (c == '/') {
    char t = get_char();
    if (t == '>') {
      // self closing tag
      c_scan = &scanner::scan_body;
      return TT_TAG_END;
    } else {
      push_back(t);
      return TT_ERROR;
    }  // erroneous situtation - standalone '/'
  }

  attr_name_.clear();
  value_.clear();

  // attribute name...
  while (c != '=') {
    if (c == 0) return TT_EOF;
    if (c == '>') {
      push_back(c);
      return TT_ATTR;
    }  // attribute without value (HTML style)
    if (is_whitespace(c)) {
      c = skip_whitespace();
      if (c != '=') {
        push_back(c);
        return TT_ATTR;
      }  // attribute without value (HTML style)
      else
        break;
    }
    if (c == '<') return TT_ERROR;
    attr_name_.push_back(c);
    c = get_char();
  }

  c = skip_whitespace();
  // attribute value...

  if (c == '\"') {
    c = get_char();
    while (c) {
      if (c == '\"') return TT_ATTR;
      // if (c == '&') c = scan_entity();
      value_.push_back(c);
      c = get_char();
    }
  } else if (c == '\'')  // allowed in html
  {
    c = get_char();
    while (c) {
      if (c == '\'') return TT_ATTR;
      // if (c == '&') c = scan_entity();
      value_.push_back(c);
      c = get_char();
    }
  } else  // scan token, allowed in html: e.g. align=center
  {
    c = get_char();
    do {
      if (is_whitespace(c)) return TT_ATTR;
      /* these two removed in favour of better html support:
      if( c == '/' || c == '>' ) { push_back(c); return TT_ATTR; }
      if( c == '&' ) c = scan_entity();*/
      if (c == '>') {
        push_back(c);
        return TT_ATTR;
      }
      value_.push_back(c);
      c = get_char();
    } while (c);
  }

  return TT_ERROR;
}

// caller already consumed '<'
// scan header start or tag tail
scanner::token_type scanner::scan_tag() {
  tag_name_.clear();

  char c = get_char();

  bool is_tail = c == '/';
  if (is_tail) c = get_char();

  while (c) {
    if (is_whitespace(c)) {
      c = skip_whitespace();
      break;
    }

    if (c == '/' || c == '>') break;

    tag_name_.push_back(c);

    if (tag_name_ == "!--") {
      c_scan = &scanner::scan_comment;
      return TT_COMMENT_START;
    }

    if (tag_name_ == "![CDATA[") {
      c_scan = &scanner::scan_cdata;
      return TT_CDATA_START;
    }

    if (tag_name_ == "!ENTITY") {
      c_scan = &scanner::scan_entity_decl;
      return TT_ENTITY_START;
    }

    c = get_char();
  }

  if (c == 0) return TT_ERROR;

  if (is_tail) return c == '>' ? TT_TAG_END : TT_ERROR;

  push_back(c);
  c_scan = &scanner::scan_head;
  return TT_TAG_START;
}

scanner::token_type scanner::scan_entity() {
  // note that when scan_entity() is called, & is already consumed.

  std::string buffer;
  buffer.clear();
  buffer.push_back('&');  // (just makes resolve_entity and value_.append(buffer) easier)

  bool has_end = false;

  while (true) {
    char c = get_char();
    buffer.push_back(c);

    // Found end of entity
    if (c == ';') break;

    // Too long to be entity
    if (buffer.size() == MAX_ENTITY_SIZE) break;

    // Not a character we'd expect in an entity (esp '&' or '<')
    if (!isalpha(c)) break;
  }

  // Keep the text_end that scanner::scan_body uses similarly up-to-date. Since
  // scan_entity() is only called from scan_body we assume text_begin is already
  // set correctly by it.
  text_end += buffer.size();

  // If we found the end of the entity, and we can identify it, then
  // resolve_entity() will emit the char it encoded.
  if (buffer.back() == ';' && resolve_entity(buffer)) return TT_TEXT;

  // Otherwise, we just emit whatever we read as text, except for the last
  // character that caused us to break. That may be another &, or a <, which we
  // would want to scan properly.
  value_.append(buffer.data(), buffer.size() - 1);
  push_back(buffer.back());
  --text_end;  // because push_back()
  return TT_TEXT;
}

bool scanner::resolve_entity(std::string const &buffer) {
  if (buffer == "&lt;") {
    value_.push_back('<');
    return true;
  }
  if (buffer == "&gt;") {
    value_.push_back('>');
    return true;
  }
  if (buffer == "&amp;") {
    value_.push_back('&');
    return true;
  }
  if (buffer == "&quot;") {
    value_.push_back('"');
    return true;
  }
  if (buffer == "&apos;") {
    value_.push_back('\'');
    return true;
  }
  if (buffer == "&nbsp;") {
    value_.push_back(' ');  // TODO: handle non-breaking spaces better than just converting them to spaces
    return true;
  }
  return false;
}

// skip whitespaces.
// returns first non-whitespace char
char scanner::skip_whitespace() {
  while (char c = get_char()) {
    if (!is_whitespace(c)) return c;
  }
  return 0;
}

void scanner::push_back(char c) {
  assert(input_char == 0);
  input_char = c;
}

char scanner::get_char() {
  if (input_char) {
    char t(input_char);
    input_char = 0;
    return t;
  }
  return input.get_char();
}

bool scanner::is_whitespace(char c) {
  return c <= ' ' && (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f');
}

scanner::token_type scanner::scan_comment() {
  if (got_tail) {
    c_scan = &scanner::scan_body;
    got_tail = false;
    return TT_COMMENT_END;
  }
  value_.clear();
  while (true) {
    char c = get_char();
    if (c == 0) return TT_EOF;
    value_.push_back(c);

    if (ends_with(value_, "-->")) {
      got_tail = true;
      value_.erase(value_.end() - 3);
      break;
    }
  }
  return TT_DATA;
}

scanner::token_type scanner::scan_special() {
  if (got_tail) {
    c_scan = &scanner::scan_body;
    got_tail = false;
    return TT_TAG_END;
  }
  value_.clear();
  while (true) {
    char c = get_char();
    if (c == 0) return TT_EOF;

    value_.push_back(c);

    // Test for </tag>
    if (c == '>' && value_.size() >= tag_name_.size() + 3) {
      // Test for the "</"" bit of "</tag>"
      size_t pos_tag_start = value_.size() - tag_name_.size() - 3;
      if (std::memcmp(value_.data() + pos_tag_start, "</", 2) != 0) continue;

      // Test for the "tag" bit of "</tag>". Doing case insensitive compare because <I>...</i> is okay.
      size_t pos_tag_name = value_.size() - tag_name_.size() - 1;  // end - tag>
      if (!equals_case_insensitive(value_.data() + pos_tag_name, tag_name_.data(), tag_name_.size())) continue;

      got_tail = true;
      value_.erase(value_.begin() + pos_tag_start);
      break;
    }
  }
  return TT_DATA;
}

scanner::token_type scanner::scan_cdata() {
  if (got_tail) {
    c_scan = &scanner::scan_body;
    got_tail = false;
    return TT_CDATA_END;
  }
  value_.clear();
  while (true) {
    char c = get_char();
    if (c == 0) return TT_EOF;
    value_.push_back(c);
    if (ends_with(value_, "]]>")) {
      got_tail = true;
      value_.erase(value_.end() - 3);
      break;
    }
  }
  return TT_DATA;
}

scanner::token_type scanner::scan_pi() {
  if (got_tail) {
    c_scan = &scanner::scan_body;
    got_tail = false;
    return TT_PI_END;
  }
  value_.clear();
  while (true) {
    char c = get_char();
    if (c == 0) return TT_EOF;
    value_.push_back(c);
    if (ends_with(value_, "?>")) {
      got_tail = true;
      value_.erase(value_.end() - 2);
      break;
    }
  }
  return TT_DATA;
}

scanner::token_type scanner::scan_entity_decl() {
  if (got_tail) {
    c_scan = &scanner::scan_body;
    got_tail = false;
    return TT_ENTITY_END;
  }
  value_.clear();
  char t;
  unsigned int tc = 0;
  while (true) {
    t = get_char();
    if (t == 0) return TT_EOF;
    value_.push_back(t);
    if (t == '\"')
      tc++;
    else if (t == '>' && (tc & 1u) == 0) {
      got_tail = true;
      break;
    }
  }
  return TT_DATA;
}

}  // namespace markup
