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

    TT_TAG_START,      // <tag ...
                       //     ^-- happens here
                       //
    TT_TAG_END,        // </tag>
                       //       ^-- happens here
                       // <tag ... />
                       //            ^-- or here
                       //
    TT_ATTR,           // <tag attr="value" >
                       //                 ^-- happens here, attr_name() and value()
                       //                     will be filled with 'attr' and 'value'.
                       //
    TT_TEXT,           // <tag>xxx</tag>
                       //         ^-- happens here
                       // <tag>foo &amp;&amp; bar</tag>
                       //          ^---^----^----^-- and all of here as well
                       // Comes after TT_TAG_START or as the first token if the input
                       // begins with text instead of a root element.
                       //
    TT_DATA,           // <!-- foo -->
                       //         ^-- here
                       // <? ... ?>
                       //       ^-- as well as here
                       // <script>...</script>
                       //            ^-- or here
                       // <style>...</style>
                       //           ^-- or here
                       // comes after TT_COMMENT_START, TT_PI_START, or TT_TAG_START
                       // if the tag was <script> or <style>.
                       //
    TT_COMMENT_START,  // <!-- foo -->
                       //     ^-- happens here
                       //
    TT_COMMENT_END,    // <!-- foo -->
                       //             ^-- happens here
                       //
    TT_PI_START,       // <?xml version="1.0?>
                       //   ^-- happens here
                       //
    TT_PI_END,         // <?xml version="1.0?>
                       //                     ^-- would you believe this happens here
  };

 public:
  explicit scanner(instream &is)
      : value_{nullptr, 0}, tag_name_{nullptr, 0}, attr_name_{nullptr, 0}, input_(is), got_tail(false) {
    c_scan = &scanner::scan_body;
  }

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

  // Consumes <?name [attrs]?>
  token_type scan_pi();

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
