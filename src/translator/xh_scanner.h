// https://www.codeproject.com/Articles/14076/Fast-and-Compact-HTML-XML-Scanner-Tokenizer
// BSD license
//|
//| simple and fast XML/HTML scanner/tokenizer
//|
//| (C) Andrew Fedoniouk @ terrainformatica.com
//|
#include <cassert>
#include <cstring>
#include <string_view>

namespace markup {

struct instream {
  const char *p;
  const char *begin;
  const char *end;
  explicit instream(const char *src) : p(src), begin(src), end(src + strlen(src)) {}
  instream(const char *begin, const char *end) : p(begin), begin(begin), end(end) {}
  char consume() { return p < end ? *p++ : 0; }
  char peek() const { return p < end ? *p : 0; }
  const char *pos() const { return p; }
};

// Think string_view, but with a mutable range
struct string_ref {
  const char *data;
  size_t size;
};

class scanner {
 public:
  enum token_type {
    TT_ERROR = -1,
    TT_EOF = 0,

    TT_TAG_START,  // <tag ...
    //     ^-- happens here
    TT_TAG_END,  // </tag>
    //       ^-- happens here
    // <tag ... />
    //            ^-- or here
    TT_ATTR,  // <tag attr="value" >
    //                  ^-- happens here
    TT_TEXT,

    TT_DATA,  // content of followings:
    // (also content of TT_TAG_START and TT_TAG_END, if the tag is 'script' or 'style')

    TT_COMMENT_START,
    TT_COMMENT_END,  // after "<!--" and "-->"
  };

 public:
  explicit scanner(instream &is) : input_(is), got_tail(false) { c_scan = &scanner::scan_body; }

  // get next token
  token_type next_token() { return (this->*c_scan)(); }

  // get value of TT_TEXT, TT_ATTR and TT_DATA
  std::string_view value() const;

  // get attribute name
  std::string_view attr_name() const;

  // get tag name
  std::string_view tag_name() const;

 private: /* methods */
  typedef token_type (scanner::*scan)();

  scan c_scan;  // current 'reader'

  // Consumes the text around and between tags
  token_type scan_body();

  // Consumes name="attr"
  token_type scan_attr();

  // Consumes <!-- ... -->
  token_type scan_comment();

  // Consumes ...</style> and ...</script>
  token_type scan_special();

  // Consumes <tagname and </tagname>
  token_type scan_tag();

  // Consumes '&amp;' etc, emits parent_token_type
  token_type scan_entity(token_type parent_token_type);

  size_t skip_whitespace();

  bool resolve_entity(string_ref const &buffer, string_ref &decoded) const;

  static bool is_whitespace(char c);

 private: /* data */
  string_ref value_;
  string_ref tag_name_;
  string_ref attr_name_;

  instream &input_;

  bool got_tail;  // aux flag used in scan_comment
};
}  // namespace markup
