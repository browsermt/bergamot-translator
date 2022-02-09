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

class Scanner {
 public:
  enum TokenType {
    TT_ERROR = -1,
    TT_EOF = 0,

    TT_TAG_START,                     // <tag ...
                                      //     ^-- happens here
                                      //
    TT_TAG_END,                       // </tag>
                                      //       ^-- happens here
                                      // <tag ... />
                                      //            ^-- or here
                                      //
    TT_ATTRIBUTE,                     // <tag attr="value" >
                                      //                 ^-- happens here, attr_name() and value()
                                      //                     will be filled with 'attr' and 'value'.
                                      //
    TT_TEXT,                          // <tag>xxx</tag>
                                      //         ^-- happens here
                                      // <tag>foo &amp;&amp; bar</tag>
                                      //          ^---^----^----^-- and all of here as well
                                      // Comes after TT_TAG_START or as the first token if the input
                                      // begins with text instead of a root element.
                                      //
    TT_DATA,                          // <!-- foo -->
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
    TT_COMMENT_START,                 // <!-- foo -->
                                      //     ^-- happens here
                                      //
    TT_COMMENT_END,                   // <!-- foo -->
                                      //             ^-- happens here
                                      //
    TT_PROCESSING_INSTRUCTION_START,  // <?xml version="1.0?>
                                      //   ^-- happens here
                                      //
    TT_PROCESSING_INSTRUCTION_END,    // <?xml version="1.0?>
                                      //                     ^-- would you believe this happens here
  };

 public:
  explicit Scanner(instream &is)
      : value_{nullptr, 0},
        tagName_{nullptr, 0},
        attributeName_{nullptr, 0},
        input_(is),
        start_(nullptr),
        scanFun_(&Scanner::scanBody),
        gotTail_(false) {}

  // get next token
  TokenType next() { return (this->*scanFun_)(); }

  // get value of TT_TEXT, TT_ATTR and TT_DATA
  std::string_view value() const;

  // get attribute name
  std::string_view attribute() const;

  // get tag name
  std::string_view tag() const;

  inline const char *start() const { return start_; }

 private: /* methods */
  typedef TokenType (Scanner::*ScanPtr)();

  // Consumes the text around and between tags
  TokenType scanBody();

  // Consumes name="attr"
  TokenType scanAttribute();

  // Consumes <!-- ... -->
  TokenType scanComment();

  // Consumes <?name [attrs]?>
  TokenType scanProcessingInstruction();

  // Consumes ...</style> and ...</script>
  TokenType scanSpecial();

  // Consumes <tagname and </tagname>
  TokenType scanTag();

  // Consumes '&amp;' etc, emits parent_token_type
  TokenType scanEntity(TokenType parentTokenType);

  size_t skipWhitespace();

  bool resolveEntity(string_ref const &buffer, string_ref &decoded) const;

  static bool isWhitespace(char c);

 private: /* data */
  string_ref value_;
  string_ref tagName_;
  string_ref attributeName_;

  ScanPtr scanFun_;  // current 'reader'

  instream &input_;

  // Start position of a token.
  const char *start_;

  bool gotTail_;  // aux flag used in scanComment, scanSpecial, scanProcessingInstruction
};
}  // namespace markup
