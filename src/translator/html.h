#ifndef SRC_BERGAMOT_HTML_H_
#define SRC_BERGAMOT_HTML_H_

#include <forward_list>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

#include "annotation.h"
#include "data/types.h"
#include "definitions.h"

namespace marian::bergamot {

struct Response;

/// HTML class parses and removes HTML from input text, and places it back into
/// the translated output text.
///
/// When parsing the HTML, it treats tags as markup, where a list of nested tags
/// can be seen as a list of markups that are applicable to all the text that
/// follows. This list is stored as a `TagStack`. Whenever an HTML tag opens or
/// closes, a new TagStack is created to reflect that. TagStack used to be
/// called `Taint` because it *tainted* the text it was associated with with
/// those tags as markup. The text between tags themselves is stored in the
/// input variable. In `spans_`, the TagStack that is associated with a
/// substring of that text is stored.
/// When transferring the HTML from the source text to the translated target
/// text, the TagStacks are first associated with each of the subwords from the
/// source text. Using hard alignment, each subword in the source text is linked
/// to a subword in the target text. The TagStacks are then copied over these
/// links. Finally, the HTML is inserted back into the target text by for each
/// subword, comparing the TagStack from the previous word to that word, and
/// opening and closing elements to make up for the difference.
///
/// There are a couple of complexities though:
/// 1. Not all tags can be treated as markup applied to text. For example, an
///    `<img>` does not contain text itself. Or `<i></i>` does not. We do want
///    those tags to remain in the output though. We do this by associating
///    them to an empty `Span`. When inserting HTML back into the translation
///    input or output, we keep track of where in the `spans_` vector we are,
///    and insert any elements from empty spans that we might have skipped over
///    because empty spans are never linked to tokens/subwords. These are
///    *stragglers* in some parts of the code, or *void* or *empty* elements in
///    other parts.
/// 2. Some tags should be treated as paragraph indicators, and break up
///    sentences. These are the usual suspects like `<p>`, but also `<li>` and
///    `<td>`, to make sure we don't translate two table cells into a single
///    word. This is the `addSentenceBreak` flag in the HTML parsing bit.
///    We mark these breaks with `\n\n` in the input text and with a special
///    WHITESPACE tag that we treat as any other void tag. Hopefully this tag
///    moves with the added `\n\n` and it is easy for us to remove it again.
///    (in practise it is since these only occur at the end of sentences and
///    the end of sentences are always aligned between source and target.)
/// 3. We treat most tags as word-breaking. We do this by adding spaces just
///    after where we saw the open or close tag occur. If there is already
///    some whitespace in that place, we do not add extra spaces.
/// 4. TODO
class HTML {
 public:
  using TagNameSet = std::set<std::string, std::less<>>;

  /// Options struct that controls how HTML is interpreted.
  struct Options {
    /// List of elements for which we do not expect a closing tag, or
    /// self-closing elements in XHTML. We do not need to see a closing tag
    /// for these elements, and they cannot contain text or tags themselves.
    /// See also:
    /// https://developer.mozilla.org/en-US/docs/Glossary/Empty_element.
    /// More relevant source of this list:
    /// https://searchfox.org/mozilla-central/rev/7d17fd1fe9f0005a2fb19e5d53da4741b06a98ba/dom/base/FragmentOrElement.cpp#1791
    TagNameSet voidTags{"area", "base",  "basefont", "bgsound", "br",   "col",   "embed",  "frame", "hr",
                        "img",  "input", "keygen",   "link",    "meta", "param", "source", "track", "wbr"};

    /// List of elements that are treated as inline, meaning they do not break
    /// up sentences. Any element *not* in this list will cause the text that
    /// follows its open or close tag to be treated as a separate sentence.
    TagNameSet inlineTags{"abbr",   "a", "b",    "em",    "i",    "kbd",    "mark", "math",
                          "output", "q", "ruby", "small", "span", "strong", "sub",  "sup",
                          "time",   "u", "var",  "wbr",   "ins",  "del",    "img"};

    /// List of elements that are, regardless of `substituteInlineTagsWithSpaces`,
    /// not substituted with spaces. Technically almost all inline elements
    /// should be treated like this, except `<br>` maybe, But in practice it
    /// seems to be more effective to limit this set to just that one tag that
    /// that can only really be used *inside* words: `<wbr>`.
    /// See also: https://developer.mozilla.org/en-US/docs/Web/HTML/Element/wbr
    TagNameSet inWordTags{"wbr"};

    /// List of elements we copy as is, but do parse as if they're HTML because
    /// they could be nested. For <script> we just scan for </script> because
    /// the script tag may not be nested, but that is not the case for these
    /// elements per se. Some tags, like <script>, are ignored at the `Scanner`
    /// level. See `xh_scanner.cpp/Scanner::scanAttribute()`.
    TagNameSet ignoredTags{"code", "kbd", "samp", "var", "dir", "acronym", "math"};

    /// List of characters that occur at the start of a token that indicate that
    /// the this token is probably *not* a continuation of a word. This is also
    /// used to determine whether there should be a space after a closing tag
    /// or not. I.e. a `.` after a `</strong>` does not need to be separated by
    /// an extra space.
    std::string continuationDelimiters = "\n ,.(){}[]";

    /// Should we always add spaces to the places where tags used to be? I.e.
    /// `un<u>der</u>line` should become `un der line`? This does help with
    /// retaining tags inside words, or with odd pages that use CSS to add
    /// spacing between a lot of tags. Cases like `<td>` and `<li>` are already
    /// covered by treating them as sentence splitting.
    bool substituteInlineTagsWithSpaces = true;
  };

  /// Represents a tag, or markup that is being applied to a string of text.
  /// We treat all elements except `ELEMENT` as void elements or empty elements.
  struct Tag {
    enum NodeType {
      ELEMENT,                 // <b>...</b>
      VOID_ELEMENT,            // <img>
      COMMENT,                 // <!-- ... -->
      PROCESSING_INSTRUCTION,  // <?...?>
      WHITESPACE,              // A \n\n we inserted to break a sentence.
    };

    NodeType type;           // Type of the node
    std::string name;        // Tag name (if type is ELEMENT or VOID_ELEMENT)
    std::string attributes;  // Tag attributes (as raw HTML string, including
                             // entities and prefix whitespace)
    std::string data;        // Raw data of an element that just needs to be
                             // copied as is, e.g. <script> or <style>
  };

  /// Representation of markup that is being applied to a string of text. Order
  /// matters as this represents how the tags are nested. The `Tag` objects
  /// themselves are owned by `pool_`.
  using TagStack = std::vector<Tag *>;

  /// Span of text, with which a `TagStack` is associated. A span may be empty,
  /// for example to represent the presence of an empty or VOID element.
  struct Span {
    size_t begin;   // Start offset in (plain text) source
    size_t end;     // end offset in source
    TagStack tags;  // Note: free pointers to memory owned by `pool_`.
    inline size_t size() const { return end - begin; }
  };

  /// Parses HTML in `source` (if `processMarkup` is true). `source` is updated
  /// to only contain the plain text extracted from the HTML. `HTML` instance
  /// retains information about what tags are extracted from where to later
  /// reconstruct the HTML in a `Response` object (both `source` and `target`).
  explicit HTML(std::string &&source, bool processMarkup) : HTML(std::move(source), processMarkup, HTML::Options{}){};
  explicit HTML(std::string &&source, bool processMarkup, Options &&options);

  /// It is not save to copy a HTML instance.
  HTML(const HTML &) = delete;

  /// Moving is fine
  HTML(HTML &&) = default;

  /// Reconstructs (not perfectly) the HTML as it was parsed from `source`,
  /// and uses alignment information to also reconstruct the same markup in
  /// `response.target`.
  void restore(Response &response);

 private:
  using SpanIterator = std::vector<HTML::Span>::iterator;
  using AnnotatedText = marian::bergamot::AnnotatedText;

  /// Reconstructs HTML in `response.source` (passed as `in`) and makes a list
  /// `sourceTokenSpans` that associates a `Span` with each subword in `in`.
  /// We later use these span pointers to copy tags. They're iterators (or
  /// pointers into a list) to be able to compare whether one span came before
  /// or after another span.
  AnnotatedText restoreSource(AnnotatedText const &in, std::vector<SpanIterator> &sourceTokenSpans);

  /// Inserts the HTML into `response.target` (passed as `in`) based on
  /// `targetTokenSpans`, which points to a `Span` for each token (subword) in
  /// `response.target`.
  AnnotatedText restoreTarget(AnnotatedText const &in, std::vector<SpanIterator> const &targetTokenSpans,
                              std::vector<HTML::TagStack> const &targetTokenTags);

  /// Utilities to test whether subword `str` is part of a word together with
  /// the subword `prev`, or a separate word. Basically *does `str` start with
  /// a space, but bit more complex to deal with punctuation.
  bool isContinuation(marian::string_view prev, marian::string_view str) const;
  bool isContinuation(std::string_view prev, std::string_view str) const;

  /// Copies span pointers from the subwords/tokens from the source text to the
  /// subwords of the target text in `targetTokenSpans` using alignment
  /// information in `response`.
  void copyTagStack(Response const &response, std::vector<std::vector<size_t>> const &alignments,
                    std::vector<HTML::SpanIterator> const &sourceTokenSpans,
                    std::vector<HTML::SpanIterator> &targetTokenSpans);

  void annotateTagStack(Response const &response, std::vector<SpanIterator> const &targetTokenSpans,
                        std::vector<HTML::TagStack> &targetTokenTags);

  /// Turns the alignment scores in `response.alignments` into one source token
  /// per target token. Has some heuristics to keep all target tokens of a
  /// single word pointing to the same span, and prefers spans with more markup
  /// over spans with less to try to retain as much of the input markup as
  /// possible.
  void hardAlignments(Response const &response, std::vector<std::vector<size_t>> &alignments,
                      std::vector<HTML::SpanIterator> const &sourceTokenSpans);

  /// Allocates a tag in `pool_` (which then owns it) and gives a pointer to be
  /// used in TagStacks. Pointer is valid as long as this HTML instance lives on.
  Tag *makeTag(Tag &&tag);

  /// HTML options associated with this parse.
  Options options_;

  /// List of spans of text in plain text `source`, and which tags are applied
  /// to them.
  std::vector<Span> spans_;

  /// A pool of tags. `std::forward_list` because we do not want pointers to it
  /// to be invalidated when new tags are allocated. This way it is easy to
  /// deallocate them all when `HTML` goes out of scope.
  std::forward_list<Tag> pool_;
};

}  // namespace marian::bergamot

#endif  // SRC_BERGAMOT_HTML_H_
