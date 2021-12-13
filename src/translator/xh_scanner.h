// https://www.codeproject.com/Articles/14076/Fast-and-Compact-HTML-XML-Scanner-Tokenizer
// BSD license
//|
//| simple and fast XML/HTML scanner/tokenizer
//|
//| (C) Andrew Fedoniouk @ terrainformatica.com
//|
#include <cstring>
#include <string>

namespace markup {
struct instream {
  const char *p;
  const char *end;
  explicit instream(const char *src) : p(src), end(src + strlen(src)) {}
  instream(const char *begin, const char *end) : p(begin), end(end) {}
  char get_char() { return p < end ? *p++ : 0; }
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
    TT_CDATA_START,
    TT_CDATA_END,  // after "<![CDATA[" and "]]>"
    TT_PI_START,
    TT_PI_END,  // after "<?" and "?>"
    TT_ENTITY_START,
    TT_ENTITY_END,  // after "<!ENTITY" and ">"

  };

  enum $ { MAX_ENTITY_SIZE = 8 };

 public:
  explicit scanner(instream &is) : input(is), input_char(0), got_tail(false) { c_scan = &scanner::scan_body; }

  // get next token
  token_type get_token() { return (this->*c_scan)(); }

  // get text span backed by original input.
  const char *get_text_begin() const { return text_begin; }
  const char *get_text_end() const { return text_end; }

  // get value of TT_TEXT, TT_ATTR and TT_DATA
  std::string const &get_value() const;

  // get attribute name
  std::string const &get_attr_name() const;

  // get tag name (always lowercase)
  std::string const &get_tag_name() const;

 private: /* methods */
  typedef token_type (scanner::*scan)();

  scan c_scan;  // current 'reader'

  // content 'readers'
  token_type scan_body();

  token_type scan_head();

  token_type scan_comment();

  token_type scan_cdata();

  token_type scan_special();

  token_type scan_pi();

  token_type scan_tag();

  token_type scan_entity();

  token_type scan_entity_decl();

  char skip_whitespace();

  void push_back(char c);

  char get_char();

  bool resolve_entity(std::string const &bufer);

  static bool is_whitespace(char c);

 private: /* data */
  std::string value_;
  std::string tag_name_;
  std::string attr_name_;

  instream &input;
  char input_char;

  bool got_tail;  // aux flag used in scan_comment, etc.

  const char *text_begin, *text_end;
};
}  // namespace markup
