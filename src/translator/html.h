#ifndef SRC_BERGAMOT_HTML_H_
#define SRC_BERGAMOT_HTML_H_

#include <forward_list>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "annotation.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

struct Response;

class BadHTML : public std::runtime_error {
 public:
  explicit BadHTML(std::string const &what) : std::runtime_error(what) {}
};

class HTML {
 public:
  struct Options {
    // List of elements for which we do not expect a closing tag, or self-closing
    // elements in XHTML. See also https://developer.mozilla.org/en-US/docs/Glossary/Empty_element
    // More relevant source of this list:
    // https://searchfox.org/mozilla-central/rev/7d17fd1fe9f0005a2fb19e5d53da4741b06a98ba/dom/base/FragmentOrElement.cpp#1791
    std::unordered_set<std::string> voidTags{"area",  "base",  "basefont", "bgsound", "br",    "col",
                                             "embed", "frame", "hr",       "img",     "input", "keygen",
                                             "link",  "meta",  "param",    "source",  "track", "wbr"};

    std::unordered_set<std::string> inlineTags{"abbr",   "a", "b",    "em",    "i",    "kbd",    "mark", "math",
                                               "output", "q", "ruby", "small", "span", "strong", "sub",  "sup",
                                               "time",   "u", "var",  "wbr",   "ins",  "del",    "img"};

    // List of characters that occur at the start of a token that indicate that
    // the this token is probably *not* a continuation of a word. Set to empty
    // to never mark a token as a continuation of the word.
    std::string continuationDelimiters = "\n ,.(){}[]";

    // Should we always add spaces to the places where tags used to be? I.e.
    // `un<u>der</u>line` should become `un der line`?
    bool substituteInlineTagsWithSpaces = true;
  };

  struct Tag {
    enum NodeType {
      ELEMENT,
      VOID_ELEMENT,
      COMMENT,
      PROCESSING_INSTRUCTION,
      WHITESPACE,  // negative space
    };

    NodeType type;           // Type of the node
    std::string name;        // Tag name (if type is ELEMENT or VOID_ELEMENT)
    std::string attributes;  // Tag attributes (as raw HTML string, including
                             // entities and prefix whitespace)
    std::string data;        // Raw data of an element that just needs to be
                             // copied as is, e.g. <script> or <style>
    // @TODO: if the original HTML stays in memory, we could replace
    // `attributes` and `data` with string_views pointing to it.
  };

  using Taint = std::vector<Tag *>;

  struct Span {
    size_t begin;
    size_t end;
    Taint tags;  // Note: free pointers! Lifetime of tags is managed by pool_
    inline size_t size() const { return end - begin; }
  };

  explicit HTML(std::string &&source, bool process_markup) : HTML(std::move(source), process_markup, HTML::Options{}){};
  explicit HTML(std::string &&source, bool process_markup, Options &&options);
  void restore(Response &response);

 private:
  using SpanIterator = std::vector<HTML::Span>::const_iterator;
  using AnnotatedText = marian::bergamot::AnnotatedText;

  AnnotatedText restoreSource(AnnotatedText const &in, std::vector<SpanIterator> &sourceTokenSpans);
  AnnotatedText restoreTarget(AnnotatedText const &in, std::vector<SpanIterator> const &targetTokenSpans);
  void copyTaint(Response const &response, std::vector<std::vector<size_t>> const &alignments,
                 std::vector<HTML::SpanIterator> const &sourceTokenSpans,
                 std::vector<HTML::SpanIterator> &targetTokenSpans);
  void hardAlignments(Response const &response, std::vector<std::vector<size_t>> &alignments);
  bool isContinuation(string_view prev, string_view str);
  // Allocates tag in pool_ (which then owns it) and gives a pointer to be used
  // in Taints. Pointer is valid as long as this HTML instance lives on.
  Tag *makeTag(Tag &&tag);

  Options options_;

  // List of text spans, and which tags are applied to them
  std::vector<Span> spans_;

  // a pool of tags that we free when HTML goes out of scope
  std::forward_list<Tag> pool_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_HTML_H_
