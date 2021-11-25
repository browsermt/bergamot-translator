// https://www.codeproject.com/Articles/14076/Fast-and-Compact-HTML-XML-Scanner-Tokenizer
// BSD license

#include "xh_scanner.h"

#include <cctype>
#include <cstring>

namespace markup {

// case sensitive string equality test
// s_lowcase shall be lowercase string
inline bool equal(const char *s, const char *s1, size_t length) { return strncmp(s, s1, length) == 0; }

const char *scanner::get_value() {
  value[value_length] = 0;
  return value;
}

const char *scanner::get_attr_name() {
  attr_name[attr_name_length] = 0;
  return attr_name;
}

const char *scanner::get_tag_name() {
  tag_name[tag_name_length] = 0;
  return tag_name;
}

scanner::token_type scanner::scan_body() {
  text_begin = input.p;
  if (input_char) {
    --text_begin;
  }
  text_end = text_begin;
  value_length = 0;
  char c = get_char();

  if (c == 0)
    return TT_EOF;
  else if (c == '<')
    return scan_tag();
  else if (c == '&')
    return scan_entity();

  while (true) {
    append_value(c);
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
    if (equal(tag_name, "script", 6)) {
      // script is special because we want to parse the attributes,
      // but not the content
      c_scan = &scanner::scan_special;
      return scan_special();
    } else if (equal(tag_name, "style", 5)) {
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

  attr_name_length = 0;
  value_length = 0;

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
    append_attr_name(c);
    c = get_char();
  }

  c = skip_whitespace();
  // attribute value...

  if (c == '\"') {
    c = get_char();
    while (c) {
      if (c == '\"') return TT_ATTR;
      // if (c == '&') c = scan_entity();
      append_value(c);
      c = get_char();
    }
  } else if (c == '\'')  // allowed in html
  {
    c = get_char();
    while (c) {
      if (c == '\'') return TT_ATTR;
      // if (c == '&') c = scan_entity();
      append_value(c);
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
      append_value(c);
      c = get_char();
    } while (c);
  }

  return TT_ERROR;
}

// caller already consumed '<'
// scan header start or tag tail
scanner::token_type scanner::scan_tag() {
  tag_name_length = 0;

  char c = get_char();

  bool is_tail = c == '/';
  if (is_tail) c = get_char();

  while (c) {
    if (is_whitespace(c)) {
      c = skip_whitespace();
      break;
    }
    if (c == '/' || c == '>') break;
    append_tag_name(c);

    switch (tag_name_length) {
      case 3:
        if (equal(tag_name, "!--", 3)) {
          c_scan = &scanner::scan_comment;
          return TT_COMMENT_START;
        }
        break;
      case 8:
        if (equal(tag_name, "![CDATA[", 8)) {
          c_scan = &scanner::scan_cdata;
          return TT_CDATA_START;
        }
        break;
      case 7:
        if (equal(tag_name, "!ENTITY", 8)) {
          c_scan = &scanner::scan_entity_decl;
          return TT_ENTITY_START;
        }
        break;
    }

    c = get_char();
  }

  if (c == 0) return TT_ERROR;

  if (is_tail) {
    if (c == '>') return TT_TAG_END;
    return TT_ERROR;
  } else
    push_back(c);

  c_scan = &scanner::scan_head;
  return TT_TAG_START;
}

scanner::token_type scanner::scan_entity() {
  // note that when scan_entity() is called, & is already consumed.

  char buffer[8];
  unsigned int buflen = 0;
  buffer[buflen++] = '&';  // (just makes resolve_entity and append_value(buffer) easier)

  bool has_end = false;

  while (true) {
    char c = get_char();
    buffer[buflen++] = c;

    // Found end of entity
    if (c == ';') break;

    // Too long to be entity
    if (buflen == sizeof(buffer)) break;

    // Not a character we'd expect in an entity (esp '&' or '<')
    if (!isalpha(c)) break;
  }

  // Keep the text_end that scanner::scan_body uses similarly up-to-date. Since
  // scan_entity() is only called from scan_body we assume text_begin is already
  // set correctly by it.
  text_end += buflen;

  // If we found the end of the entity, and we can identify it, then
  // resolve_entity() will emit the char it encoded.
  if (buffer[buflen - 1] == ';' && resolve_entity(buffer, buflen)) {
    return TT_TEXT;
  }

  // Otherwise, we just emit whatever we read as text, except for the last
  // character that caused us to break. That may be another &, or a <, which we
  // would want to scan properly.
  for (unsigned int i = 0; i < buflen - 1; ++i) append_value(buffer[i]);
  push_back(buffer[buflen - 1]);
  --text_end;  // because push_back()
  return TT_TEXT;
}

bool scanner::resolve_entity(char *buffer, unsigned int len) {
  switch (len) {
    case 4:
      if (equal(buffer, "&lt;", 4)) {
        append_value('<');
        return true;
      }
      if (equal(buffer, "&gt;", 4)) {
        append_value('>');
        return true;
      }
      break;

    case 5:
      if (equal(buffer, "&amp;", 5)) {
        append_value('&');
        return true;
      }
      break;

    case 6:
      if (equal(buffer, "&quot;", 6)) {
        append_value('"');
        return true;
      }
      if (equal(buffer, "&apos;", 6)) {
        append_value('\'');
        return true;
      }
      if (equal(buffer, "&nbsp;", 6)) {
        append_value(' ');  // TODO: handle non-breaking spaces better than just converting them to spaces
        return true;
      }
      break;
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

void scanner::push_back(char c) { input_char = c; }

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

void scanner::append_value(char c) {
  if (value_length < (MAX_TOKEN_SIZE - 1)) value[value_length++] = c;
}

void scanner::append_attr_name(char c) {
  if (attr_name_length < (MAX_NAME_SIZE - 1)) attr_name[attr_name_length++] = char(c);
}

void scanner::append_tag_name(char c) {
  if (tag_name_length < (MAX_NAME_SIZE - 1))
    tag_name[tag_name_length++] =
        std::tolower(static_cast<unsigned char>(c));  // cast because std::tolower has undefined behaviour otherwise
}

scanner::token_type scanner::scan_comment() {
  if (got_tail) {
    c_scan = &scanner::scan_body;
    got_tail = false;
    return TT_COMMENT_END;
  }
  for (value_length = 0; value_length < (MAX_TOKEN_SIZE - 1); ++value_length) {
    char c = get_char();
    if (c == 0) return TT_EOF;
    value[value_length] = c;

    if (value_length >= 2 && value[value_length] == '>' && value[value_length - 1] == '-' &&
        value[value_length - 2] == '-') {
      got_tail = true;
      value_length -= 2;
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
  for (value_length = 0; value_length < (MAX_TOKEN_SIZE - 1); ++value_length) {
    char c = get_char();
    if (c == 0) return TT_EOF;

    // in case MAX_TOKEN_SIZE limit breaks up the end tag
    if (c == '<' && value_length + tag_name_length + 3 >= MAX_TOKEN_SIZE) {
      push_back(c);
      break;
    }

    value[value_length] = c;

    if (c == '>' && value_length >= tag_name_length + 2) {
      unsigned int i = tag_name_length - 1;
      do {
        if (value[value_length + i - tag_name_length] != tag_name[i]) break;
        --i;
      } while (i > 0);
      if (i > 0) continue;
      if (value[value_length - tag_name_length - 1] != '/') continue;
      if (value[value_length - tag_name_length - 2] != '<') continue;

      got_tail = true;
      value_length = value_length - tag_name_length - 2;
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
  for (value_length = 0; value_length < (MAX_TOKEN_SIZE - 1); ++value_length) {
    char c = get_char();
    if (c == 0) return TT_EOF;
    value[value_length] = c;

    if (value_length >= 2 && value[value_length] == '>' && value[value_length - 1] == ']' &&
        value[value_length - 2] == ']') {
      got_tail = true;
      value_length -= 2;
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
  for (value_length = 0; value_length < (MAX_TOKEN_SIZE - 1); ++value_length) {
    char c = get_char();
    if (c == 0) return TT_EOF;
    value[value_length] = c;

    if (value_length >= 1 && value[value_length] == '>' && value[value_length - 1] == '?') {
      got_tail = true;
      value_length -= 1;
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
  char t;
  unsigned int tc = 0;
  for (value_length = 0; value_length < (MAX_TOKEN_SIZE - 1); ++value_length) {
    t = get_char();
    if (t == 0) return TT_EOF;
    value[value_length] = t;
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
