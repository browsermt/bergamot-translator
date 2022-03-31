#include "html.h"

#include <algorithm>

#include "response.h"
#include "translator/definitions.h"
#include "xh_scanner.h"

namespace {
using marian::bergamot::AnnotatedText;
using marian::bergamot::ByteRange;
using marian::bergamot::HTML;
using marian::bergamot::Response;

/// Encodes the minimum of HTML entities.
void encodeEntities(marian::string_view const &input, std::string &output) {
  output.clear();
  output.reserve(input.size());  // assumes there are no entities in most cases

  for (char it : input) {
    switch (it) {
      case '&':
        output.append("&amp;");
        break;
      case '<':
        output.append("&lt;");
        break;
      case '>':
        output.append("&gt;");
        break;
      // case ???:
      //   output.append("&nbsp;");
      //   break;
      // case '"':
      //   output.append("&quot;");
      //   break;
      // case '\'':
      //   output.append("&apos;");
      //   break;
      default:
        output.push_back(it);
        break;
    }
  }
}

/// Counts number of whitespace characters at the start of the input. Used
/// for determining where to insert an open or close tag.
size_t countPrefixWhitespaces(marian::string_view const &input) {
  size_t size = 0;
  while (size < input.size() && std::isspace(static_cast<unsigned char>(input[size]))) ++size;
  return size;
}

std::string toLowerCase(std::string_view const &input) {
  std::string out;
  out.resize(input.size());
  std::transform(input.begin(), input.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
  return out;
}

/// Very simple replacement for std::format introduced in C++20. Only supports
/// replacing `{}` in the template string with whatever `operator<<` for that
/// type turns it into.
std::string format(std::string const &formatTemplate) { return formatTemplate; }

template <typename Arg>
std::string format(std::string const &formatTemplate, Arg arg) {
  std::ostringstream os;
  auto index = formatTemplate.find("{}");
  assert(index != std::string::npos);
  os << formatTemplate.substr(0, index) << arg << formatTemplate.substr(index + 2);
  return os.str();
}

template <typename Arg, typename... Args>
std::string format(std::string const &formatTemplate, Arg arg, Args... args) {
  std::ostringstream os;
  auto index = formatTemplate.find("{}");
  assert(index != std::string::npos);
  os << formatTemplate.substr(0, index) << arg << format(formatTemplate.substr(index + 2), std::forward<Args>(args)...);
  return os.str();
}

/// Syntactic sugar around rbegin() and rend() that allows me to write
/// `for (auto &&item : reversed(container))` instead of the needlessly verbose
/// `for (auto it = container.rbegin(); it != container.rend(); ++it)`
template <typename T>
class Reversed {
 public:
  using iterator = typename T::const_reverse_iterator;
  explicit Reversed(T const &container) : container_(container){};
  iterator begin() const { return container_.rbegin(); }
  iterator end() const { return container_.rend(); }

 private:
  T const &container_;
};

/// When comparing two tag stacks, determine which tags need to be closed and
/// opened to get from one stack to the other.
void diffTags(HTML::TagStack const &prev, HTML::TagStack const &curr, HTML::TagStack &opening,
              HTML::TagStack &closing) {
  opening.clear();
  closing.clear();

  size_t i = 0;

  // Find first difference
  for (; i < prev.size(); ++i)
    if (i >= curr.size() || prev[i] != curr[i]) break;

  // Only nodes of type ELEMENT can have children and thus would need a closing tag.
  // NOLINTNEXTLINE(bugprone-narrowing-conversions)
  std::copy_if(prev.begin() + i, prev.end(), std::back_inserter(closing),
               [&](HTML::Tag *tag) { return tag->type == HTML::Tag::ELEMENT; });

  // NOLINTNEXTLINE(bugprone-narrowing-conversions)
  opening.insert(opening.end(), curr.begin() + i, curr.end());
}

bool intersects(ByteRange const &range, HTML::Span const &span) {
  return range.begin <= span.end && range.end >= span.begin;
};

bool contains(HTML::TagNameSet const &set, std::string_view const &name) { return set.find(name) != set.end(); }

bool contains(HTML::TagStack const &stack, HTML::Tag const *tag) {
  return std::find(stack.rbegin(), stack.rend(), tag) != stack.rend();
}

/// Is tag stack B an extended version of A? I.e. same tags, but maybe a few
/// more nested deeper.
bool extends(HTML::TagStack const &b, HTML::TagStack const &a) {
  if (a.size() > b.size()) return false;

  for (auto i = a.begin(), j = b.begin(); i != a.end(); ++i, ++j)
    if (*i != *j) return false;

  return true;
}

/// Tests whether `response` has alignment info associated with it or not.
bool hasAlignments(Response const &response) {
  // Test for each sentence individually as a sentence may be empty (or there)
  // might be no sentences, so just testing for alignments.empty() would not be
  // sufficient.
  for (size_t sentenceIdx = 0; sentenceIdx < response.target.numSentences(); ++sentenceIdx) {
    // If response.alignments is just empty, this might catch it.
    if (response.alignments.size() <= sentenceIdx ||
        response.alignments[sentenceIdx].size() != response.target.numWords(sentenceIdx))
      return false;

    // If response.alignments is "empty" because the model did not provide alignments,
    // it still has entries for each target word. But all these entries are empty.
    for (size_t wordIdx = 0; wordIdx < response.target.numWords(sentenceIdx); ++wordIdx)
      if (response.alignments[sentenceIdx][wordIdx].size() != response.source.numWords(sentenceIdx)) return false;
  }
  return true;
}

/// Helper class to append HTML tags to a token. Also makes sure the token is
/// encoded as valid HTML.
class TokenFormatter {
 public:
  explicit TokenFormatter(marian::string_view token)
      : offset_(0), whitespaceOffset_(0), whitespaceSize_(countPrefixWhitespaces(token)), closeLeft_(true) {
    // Do encoding of any entities that popped up in the translation
    encodeEntities(token, html_);
  }

  std::string &&html() { return std::move(html_); }

  // Append the markup necessary for moving from `prev` set of tags to `curr`.
  void append(HTML::TagStack const &prev, HTML::TagStack const &curr) {
    HTML::TagStack opening, closing;

    diffTags(prev, curr, opening, closing);

    for (HTML::Tag const *tag : Reversed(closing)) {
      assert(tag->type == HTML::Tag::ELEMENT);
      std::string closeTag = format("</{}>", tag->name);
      html_.insert(offset_ + (closeLeft_ ? 0 : whitespaceSize_), closeTag);
      offset_ += closeTag.size();
      if (closeLeft_) whitespaceOffset_ += closeTag.size();
    }

    for (HTML::Tag const *tag : opening) {
      std::string openTag;
      switch (tag->type) {
        case HTML::Tag::ELEMENT:
        case HTML::Tag::VOID_ELEMENT:
          openTag = format("<{}{}>{}", tag->name, tag->attributes, tag->data);
          break;
        case HTML::Tag::COMMENT:
          openTag = format("<!--{}-->", tag->data);
          break;
        case HTML::Tag::PROCESSING_INSTRUCTION:
          openTag = format("<?{}?>", tag->data);
          break;
        case HTML::Tag::WHITESPACE: {
          // Try to eat two newlines (paragraph break) from our segment
          auto pos = html_.find("\n\n", whitespaceOffset_);
          if (pos != std::string::npos && pos < whitespaceOffset_ + whitespaceSize_) {
            html_.erase(pos, 2);
            whitespaceSize_ -= 2;
          }
        } break;
      }

      html_.insert(offset_ + whitespaceSize_, openTag);
      offset_ += openTag.size();
      closeLeft_ = closeLeft_ && openTag.empty();
    }
  }

 private:
  std::string html_;         // Output html
  size_t offset_;            // Size added by prepending HTML
  size_t whitespaceOffset_;  // position of prefix whitespace characters
                             // (it moves as closing tags are prepended)
  size_t whitespaceSize_;    // number of prefix whitespace characters

  // Close tags we want to show up left (before) the token, but open tags
  // ideally come directly after any prefix whitespace. However, some tokens
  // match multiple spans. If a previous span has added an open tag, after any
  // whitespace, and the next span closes said tag again, we need to close
  // it after the whitespace. So after the first open tag, any closing tag
  // should also align right, after whitespace, not before. Hence this bool.
  bool closeLeft_;
};

/// Count the number of tokens in an AnnotatedText. Used to assert we're not
/// running out of sync when creating vectors that describe each token.
size_t debugCountTokens(AnnotatedText const &text) {
  size_t tokens = 1;  // for the ending gap
  for (size_t sentenceIdx = 0; sentenceIdx < text.numSentences(); ++sentenceIdx) {
    tokens += 1 + text.numWords(sentenceIdx);  // pre-sentence prefix/gap + each word
  }
  return tokens;
}

/// Helper function that consumes a tag as if it is a special tag, except that
/// it takes nesting into account. I.e. `<a><a></a></a>` will be consumed to the
// last `</a>`. Assumes TT_TAG_START is already consumed, which was necessary
/// to determine whether this was an element that needed to be ignored.
void consumeIgnoredTag(markup::Scanner &scanner, HTML::Tag &tag, std::string const &name) {
  // Only full elements can be consumed this way. With void tags we don't know
  // where to stop scanning. All other types cannot be nested anyway.
  assert(tag.type == HTML::Tag::ELEMENT);

  // TT_TAG_START is already consumed.
  markup::Scanner::TokenType token;
  size_t inside = 0;

  // Consume the full open tag, i.e. all its attributes
  while (!inside) {
    token = scanner.next();
    switch (token) {
      case markup::Scanner::TT_ERROR:
        ABORT("HTML parse error");
      case markup::Scanner::TT_EOF:
        ABORT("Did not find closing tag </{}>", name);
      case markup::Scanner::TT_ATTRIBUTE:
        tag.attributes += format(" {}=\"{}\"", scanner.attribute(), scanner.value());
        break;
      default:
        // Not an attribute! Must be something inside the body or the closing
        // tag already. Time to jump to the next loop.
        ++inside;
        break;
    }
  }

  // Last token was something that would have triggered Scanner::scanBody(),
  // which sets value() to start pointing at the body.
  const char *start = scanner.start();

  // Consume the rest of the HTML until (including) the final closing tag. We
  // start with the token that caused the previous loop to fall into the default
  // case.
  while (inside) {
    switch (token) {
      case markup::Scanner::TT_ERROR:
        ABORT("HTML parse error");
      case markup::Scanner::TT_EOF:
        ABORT("Did not find closing tag </{}>");
      case markup::Scanner::TT_TAG_START:
        // Note: Looking specifically for only our own type of tag so we don't
        // have to care about whether other tags we encounter are void tags or
        // not. Does assume the HTML is valid, as no stack is kept.
        if (toLowerCase(scanner.tag()) == name) ++inside;
        break;
      case markup::Scanner::TT_TAG_END:
        if (toLowerCase(scanner.tag()) == name) --inside;
        break;
      default:
        break;
    }

    // Only continue scanning if we're still inside. We could have just read the
    // TT_TAG_END token that ended this element, and we don't want to continue
    // consuming tokens at that point.
    if (inside) token = scanner.next();
  }

  // Only a TAG_END could have stopped the previous loop. We take the start
  // of the final closing tag as the end of our data.
  assert(token == markup::Scanner::TT_TAG_END);
  const char *end = scanner.start();

  // All data between the end of the first open element, and the start of the
  // last close element, we just treat as raw data that will be printed when
  // this tag is eventually printed.
  assert(end >= start);
  tag.data = std::string_view(start, end - start);
}

}  // namespace

namespace marian::bergamot {

/// Formatters used for formatting error messages in ABORT() calls.
std::ostream &operator<<(std::ostream &out, HTML::Tag const *tag) {
  if (tag == nullptr) return out << "[nullptr]";
  switch (tag->type) {
    case HTML::Tag::ELEMENT:
      return out << '<' << tag->name << tag->attributes << '>';
    case HTML::Tag::VOID_ELEMENT:
      return out << '<' << tag->name << tag->attributes << "/>";
    case HTML::Tag::COMMENT:
      return out << "<!--" << tag->data << "-->";
    case HTML::Tag::PROCESSING_INSTRUCTION:
      return out << "<?" << tag->data << "?>";
    case HTML::Tag::WHITESPACE:
      return out << "[inserted space]";
  }
  return out << "[Unknown tag type]";
}

std::ostream &operator<<(std::ostream &out, HTML::TagStack const &tags) {
  for (auto it = tags.begin(); it != tags.end(); ++it) {
    if (it != tags.begin()) out << ' ';
    out << *it;
  }
  return out;
}

HTML::HTML(std::string &&source, bool processMarkup, Options &&options) : options_(std::move(options)) {
  if (!processMarkup) return;

  std::string original = std::move(source);
  markup::instream in(original.data(), original.data() + original.size());
  markup::Scanner scanner(in);
  source.clear();  // source is moved out of, so should be clear anyway

  Tag *tag = nullptr;             // current tag (after opening at least)
  TagStack stack;                 // stack of currently open tags
  bool addSentenceBreak = false;  // whether to add a sentence break next text segment
  bool addWordBreak = false;      // whether to add a word break next text segment

  // Starting point: an empty span with no open tags.
  spans_.push_back(Span{0, 0, {}});

  bool stop = false;
  while (!stop) {
    switch (scanner.next()) {
      case markup::Scanner::TT_ERROR:
        ABORT("HTML parse error");

      case markup::Scanner::TT_EOF:
        stop = true;
        break;

      case markup::Scanner::TT_TEXT: {
        // If the previous segment was the open or close tag of a block element
        // we treat the text after it as a new sentence.
        if (addSentenceBreak) {
          // If there isn't already a \n\n at the end of source...
          if (source.size() >= 2 && source.substr(source.size() - 2) != "\n\n") {
            stack.push_back(makeTag({Tag::WHITESPACE}));
            // Important: span->size() == 0 to make it behave as a void element.
            // Also important: position before the \n\n tokens, not after, to
            // make it easier to remove them later through apply().
            spans_.push_back(Span{source.size(), source.size(), stack});
            source.append("\n\n");  // Should work with ssplit-mode = wrapped_text
            stack.pop_back();
          }
          addSentenceBreak = false;
        }

        // If the previous segment was an open or close tag, it might be best
        // to add a space to make sure we don't append to the previous word.
        if (addWordBreak) {
          // Only add the space when it would be inside a word. Do not add it if
          // it would be between a word and punctuation.
          if (options_.substituteInlineTagsWithSpaces && isContinuation(source, scanner.value())) {
            source.push_back(' ');
          }
          addWordBreak = false;
        }

        // Store which tags were open when this span of text was encountered.
        auto begin = source.size();
        source.append(scanner.value());
        spans_.push_back(Span{begin, source.size(), stack});
      } break;

      case markup::Scanner::TT_TAG_START: {
        std::string name = toLowerCase(scanner.tag());

        // Tag *tag is used by attribute parsing
        auto type = contains(options_.voidTags, name) ? Tag::VOID_ELEMENT : Tag::ELEMENT;
        tag = makeTag({type, std::string(scanner.tag())});

        stack.push_back(tag);

        // Empty elements (e.g. <img>) are not applicable to a span of text
        // so instead we "apply" them to an empty span in between, and then
        // immediately remove them again from the stack.
        if (tag->type == Tag::VOID_ELEMENT) {
          spans_.push_back(Span{source.size(), source.size(), stack});
          stack.pop_back();
        }

        // Ignored tags have same semantics as void tags with regards to moving
        // them around with the rest of the content.
        if (contains(options_.ignoredTags, name)) {
          consumeIgnoredTag(scanner, *tag, name);
          spans_.push_back(Span{source.size(), source.size(), stack});
          stack.pop_back();
        }

        // Treat non-inline HTML tags as spaces that break up words.
        if (!contains(options_.inlineTags, name)) {
          addSentenceBreak = true;
        } else if (!contains(options_.inWordTags, name)) {
          addWordBreak = true;
        }
      } break;

      case markup::Scanner::TT_TAG_END: {
        std::string tagName = toLowerCase(scanner.tag());
        // If this is the closing bit of a void tag, i.e. triggered by the "/>"
        // bit of "<img/>", then completely ignore it.
        if (contains(options_.voidTags, tagName)) break;

        ABORT_IF(stack.empty(), "Encountered more closing tags ({}) than opening tags", scanner.tag());

        ABORT_IF(toLowerCase(stack.back()->name) != toLowerCase(scanner.tag()),
                 "Encountered unexpected closing tag </{}>, stack is {}", scanner.tag(), stack);

        // What to do with "<u></u>" case, where tag is immediately closed
        // so it never makes it into the taint of any of the spans? This adds
        // an empty span so it still gets recorded in spans_.
        if (spans_.empty() || !contains(spans_.back().tags, stack.back()))
          spans_.push_back(Span{source.size(), source.size(), stack});

        stack.pop_back();

        // Add space if necessary
        if (!contains(options_.inlineTags, tagName)) {
          addSentenceBreak = true;
        } else if (!contains(options_.inWordTags, tagName)) {
          addWordBreak = true;
        }
      } break;

      case markup::Scanner::TT_ATTRIBUTE:
        assert(tag != nullptr);
        tag->attributes += format(" {}=\"{}\"", scanner.attribute(), scanner.value());
        break;

      case markup::Scanner::TT_COMMENT_START:
        // Tag *tag is used when TT_DATA is seen to add the comment's content.
        tag = makeTag({Tag::COMMENT});
        stack.push_back(tag);
        spans_.push_back(Span{source.size(), source.size(), stack});
        stack.pop_back();
        break;

      case markup::Scanner::TT_PROCESSING_INSTRUCTION_START:
        // Tag *tag is used when TT_DATA is seen to add the PI's content.
        tag = makeTag({Tag::PROCESSING_INSTRUCTION});
        stack.push_back(tag);
        spans_.push_back(Span{source.size(), source.size(), stack});
        stack.pop_back();
        break;

      case markup::Scanner::TT_COMMENT_END:
      case markup::Scanner::TT_PROCESSING_INSTRUCTION_END:
        tag = nullptr;
        break;

      case markup::Scanner::TT_DATA:
        assert(tag != nullptr);
        tag->data = scanner.value();
        break;

      default:
        ABORT("Unsupported scanner token type");
    }
  }

  ABORT_IF(!stack.empty(), "Not all tags were closed: {}", stack);

  // Add a trailing span (that's empty) to signify all closed tags.
  spans_.emplace_back(Span{source.size(), source.size(), stack});
}

void HTML::restore(Response &response) {
  // No-op if process_markup was false (and thus spans_ is empty)
  // TODO: replace this with optional<HTML> at a higher level
  if (spans_.empty()) return;

  // We need alignment info to transfer the HTML tags from the input to the
  // translation. If those are not available, no HTML in translations for you.
  ABORT_UNLESS(hasAlignments(response),
               "Response object does not contain alignments. TranslationModel or ResponseOptions is misconfigured?");

  // Reconstruction of HTML tags:
  // 1. Map each token to a Span
  // 2. Reconstruct the source HTML with these tainted tokens
  // 3. Transfer the spans from the source tokens to the target tokens using alignment information
  // 4. For spans that represent empty elements (e.g. <img>) figure out their position
  // 5. Reconstruct the target HTML with these tainted tokens

  // sourceTokenSpans is a vector with a pointer to a span for each token. We
  // use iterators here to point to these positions so we can easily compare if
  // one span comes before or after another, information we'll need when we need
  // to figure out whether we've skipped spans (of emtpy elements) when
  // reconstructing HTML in response.target.
  std::vector<SpanIterator> sourceTokenSpans;

  // RestoreSource re-inserts HTML into the source text, but also identifies
  // which span each source token fits into best.
  AnnotatedText source = restoreSource(response.source, sourceTokenSpans);
  assert(sourceTokenSpans.size() == debugCountTokens(response.source));

  // Find for every token in target the token in source that best matches.
  std::vector<std::vector<size_t>> alignments;
  hardAlignments(response, alignments, sourceTokenSpans);

  std::vector<SpanIterator> targetTokenSpans;
  copyTagStack(response, alignments, sourceTokenSpans, targetTokenSpans);
  assert(targetTokenSpans.size() == debugCountTokens(response.target));

  // Take the spans, and use them to make a taint for every word in the
  // translation. Optionally add extra tags, like quality score metadata.
  std::vector<HTML::TagStack> targetTokenTags;
  annotateTagStack(response, targetTokenSpans, targetTokenTags);

  AnnotatedText target = restoreTarget(response.target, targetTokenSpans, targetTokenTags);

  response.source = source;
  response.target = target;
}

AnnotatedText HTML::restoreSource(AnnotatedText const &in, std::vector<SpanIterator> &sourceTokenSpans) {
  auto spanIt = spans_.begin();
  auto prevIt = spans_.begin();  // safe because first span is always empty span, and
                                 // and the while-loop below will do the rest
  assert(prevIt == spans_.end() || prevIt->tags.empty());

  return in.apply([&](ByteRange range, string_view token, bool last) {
    TokenFormatter formatter(token);

    // Potential issue: spans and tokens can intersect, e.g.
    //
    //    text  <p> h <u> e </u> ll o </p>
    //   spans     |1|   |2|    |3333| (so only 2 is tainted with <p><u>, others only <p>)
    //  tokens     |111111111111111|2|
    //
    // Now 1 covers span 1 to 3, so what taint should it get? Just `<p>`, or
    // `<p><u>`?
    // Note: only relevant if `substituteInlineTagsWithSpaces` is true. If we
    // just insert spaces around all elements, every segment of `hello` will be
    // a token.

    // Seek to the last span that overlaps with this token
    while (true) {
      formatter.append(prevIt->tags, spanIt->tags);
      prevIt = spanIt;

      if (spanIt + 1 != spans_.end() && ((spanIt + 1)->begin < range.end || last)) {
        spanIt++;
        continue;
      }

      break;
    }

    // TODO: This is just the taint of the last span, not the ones in between.
    // This makes us lose some markup of parts of tokens as described above.
    sourceTokenSpans.emplace_back(prevIt);

    return std::move(formatter.html());
  });
}

AnnotatedText HTML::restoreTarget(AnnotatedText const &in, std::vector<SpanIterator> const &targetTokenSpans,
                                  std::vector<TagStack> const &targetTokenTags) {
  auto prevTags = spans_.cbegin()->tags;
  auto stragglerSpanIt = spans_.cbegin();
  auto targetSpanIt = targetTokenSpans.begin();
  auto targetTagIt = targetTokenTags.begin();

  AnnotatedText out = in.apply([&]([[maybe_unused]] ByteRange range, string_view token, bool last) {
    TokenFormatter formatter(token);

    // First we scan through spans_ to catch up to the span assigned to this
    // token. We're only interested in empty spans (empty and void elements)
    for (; stragglerSpanIt < *targetSpanIt; stragglerSpanIt++) {
      // We're only interested in empty spans or spans that would otherwise get
      // lost because they didn't align with anything between the spans in
      // targetSpanIt
      // TODO That std::find makes this O(N*N) NOT GOOD NOT GOOD
      if (stragglerSpanIt->size() != 0 &&
          std::find(targetTokenSpans.begin(), targetTokenSpans.end(), stragglerSpanIt) != targetTokenSpans.end())
        continue;

      formatter.append(prevTags, stragglerSpanIt->tags);
      prevTags = stragglerSpanIt->tags;
    }

    // Now do the same thing but for our target set of tags. Note that we cannot
    // combine this in the for-loop above (i.e. `span_it <= *targetSpanIt`)
    // because there is no guarantee that the order in `targetTokenSpans` is
    // the same as that of `spans`.

    formatter.append(prevTags, *targetTagIt);

    // If this is the last token of the response, close all open tags.
    if (last) {
      // Note: this assert is true due to our current implementation of
      // HardAlignments() that always matches the last token of the input with
      // the last token of the output. But lets assume someone someday changes
      // HardAlignments(), and then this for-loop will be necessary.
      // assert((*targetSpanIt)->tags.empty());
      formatter.append(*targetTagIt, HTML::TagStack());
    }

    prevTags = *targetTagIt;
    ++targetSpanIt;
    ++targetTagIt;

    return std::move(formatter.html());
  });

  // Assert that we did in fact use all our taints
  assert(targetSpanIt == targetTokenSpans.end());

  return out;
}

HTML::Tag *HTML::makeTag(Tag &&tag) {
  pool_.emplace_front(std::move(tag));
  return &pool_.front();
}

void HTML::copyTagStack(Response const &response, std::vector<std::vector<size_t>> const &alignments,
                        std::vector<SpanIterator> const &sourceTokenSpans,
                        std::vector<SpanIterator> &targetTokenSpans) {
  size_t offset = 0;  // Sentence offset in sourceTokenSpans

  // Fill targetTokenSpans based on the alignments we just made up.
  // NOTE: this should match the exact order of Apply()
  for (size_t sentenceIdx = 0; sentenceIdx < response.target.numSentences(); ++sentenceIdx) {
    targetTokenSpans.push_back(sourceTokenSpans[offset]);  // token_tag for sentence ending gap
    for (size_t t = 0; t < response.target.numWords(sentenceIdx); ++t) {
      size_t s = alignments[sentenceIdx][t];
      assert(s < response.source.numWords(sentenceIdx));
      targetTokenSpans.push_back(sourceTokenSpans[offset + 1 + s]);  // +1 for prefix gap
    }

    offset += response.source.numWords(sentenceIdx) + 1;  // +1 for prefix gap
  }

  assert(offset + 1 == sourceTokenSpans.size());
  targetTokenSpans.push_back(sourceTokenSpans[offset]);  // token_tag for ending whitespace
}

void HTML::annotateTagStack(Response const &response, std::vector<SpanIterator> const &targetTokenSpans,
                            std::vector<HTML::TagStack> &targetTokenTags) {
  auto spanIt = targetTokenSpans.begin();
  for (size_t sentenceIdx = 0; sentenceIdx < response.target.numSentences(); ++sentenceIdx) {
    // Sentence prefix
    targetTokenTags.push_back((*spanIt)->tags);
    spanIt++;

    // Offset in targetTokenTags at which this sentence's tags start.
    size_t tagOffset = targetTokenTags.size();

    // Initially, just copy the span's tags to this token
    for (size_t t = 0; t < response.target.numWords(sentenceIdx); ++t) {
      targetTokenTags.emplace_back((*spanIt)->tags);
      spanIt++;
    }

    // If we have quality score information, add that as metadata as well.
    if (!response.qualityScores.empty()) {
      auto const &sentenceQuality = response.qualityScores[sentenceIdx];
      // Create a single <font> tag for this sentence with sentence level info
      Tag *sentenceTag = makeTag({Tag::ELEMENT, "font"});
      sentenceTag->attributes += format(" x-bergamot-sentence-index=\"{}\" x-bergamot-sentence-score=\"{}\"",
                                        sentenceIdx, sentenceQuality.sentenceScore);

      // Add that tag to all tokens in this sentence.
      for (size_t tokenIdx = 0; tokenIdx < response.target.numWords(sentenceIdx); ++tokenIdx) {
        targetTokenTags[tagOffset + tokenIdx].push_back(sentenceTag);
      }

      // Add word level <font> tags as well to all tokens that make up a word.
      for (size_t wordIdx = 0; wordIdx < sentenceQuality.wordRanges.size(); ++wordIdx) {
        Tag *wordTag = makeTag({Tag::ELEMENT, "font"});
        wordTag->attributes += format(" x-bergamot-word-index=\"{}\" x-bergamot-word-score=\"{}\"", wordIdx,
                                      sentenceQuality.wordScores[wordIdx]);
        auto const &range = sentenceQuality.wordRanges[wordIdx];
        for (size_t tokenIdx = range.begin; tokenIdx < range.end; ++tokenIdx) {
          targetTokenTags[tagOffset + tokenIdx].push_back(wordTag);
        }
      }
    }
  }

  // Suffix
  targetTokenTags.push_back((*spanIt)->tags);
  spanIt++;

  assert(spanIt == targetTokenSpans.end());
}

// Reports if token `str` is likely to be a continuation of a word. This is used
// to determine whether we should share the markup, or whether we should see
// this token as a fresh start. This implementation will treat "hello[world]"
// as 4 words, assuming its tokenised as something like `h ell o [ wor ld ]`.
bool HTML::isContinuation(std::string_view prev, std::string_view str) const {
  if (options_.continuationDelimiters.empty()) return false;
  if (prev.empty() || str.empty()) return false;
  return options_.continuationDelimiters.find(str[0]) == std::string::npos &&
         options_.continuationDelimiters.find(prev.back()) == std::string::npos;
}

bool HTML::isContinuation(marian::string_view prev, marian::string_view str) const {
  return isContinuation(std::string_view(prev.data(), prev.size()), std::string_view(str.data(), str.size()));
}

/// Selects for each token in `response.target` a best source token from
/// `response.source` and writes this selection to `alignments`. The source
/// token spans are used to also look at the markup applied to each token to
/// figure out which source token best represents each target token.
void HTML::hardAlignments(Response const &response, std::vector<std::vector<size_t>> &alignments,
                          std::vector<SpanIterator> const &sourceTokenSpans) {
  size_t offset = 0;  // sentence offset in sourceTokenSpans

  // For each sentence...
  for (size_t sentenceIdx = 0; sentenceIdx < response.target.numSentences(); ++sentenceIdx) {
    alignments.emplace_back();

    // Hard-align: find for each target token the most prevalent source token
    // Note: only search from 0 to N-1 because token N is end-of-sentence token
    // that can only align with the end-of-sentence token of the target
    for (size_t t = 0; t + 1 < response.target.numWords(sentenceIdx); ++t) {
      alignments.back().push_back(
          std::max_element(response.alignments[sentenceIdx][t].begin(), response.alignments[sentenceIdx][t].end()) -
          response.alignments[sentenceIdx][t].begin());
    }

    // Next, we try to smooth out these selected alignments with a few heuristics
    for (size_t t = 1; t + 1 < response.target.numWords(sentenceIdx); ++t) {
      // If this token is a continuation of a previous token, pick the tags from the most
      // prevalent token for the whole word.
      if (isContinuation(response.target.word(sentenceIdx, t - 1), response.target.word(sentenceIdx, t))) {
        // Note: only looking at the previous token since that will already
        // have this treatment applied to it.
        size_t currSentenceIdx = alignments.back()[t];
        size_t prevSentenceIdx = alignments.back()[t - 1];
        float currScore = response.alignments[sentenceIdx][t][currSentenceIdx];
        float prevScore = response.alignments[sentenceIdx][t - 1][prevSentenceIdx];

        TagStack const &currTagStack = sourceTokenSpans[offset + 1 + currSentenceIdx]->tags;
        TagStack const &prevTagStack = sourceTokenSpans[offset + 1 + prevSentenceIdx]->tags;

        // If this token has more markup, or a better score than the previous
        // token (and they together are part of a word-ish thing) then mark
        // this word as aligning. Otherwise just copy the alignment source of
        // the previous token.
        if (extends(currTagStack, prevTagStack) || currScore >= prevScore) {
          // Apply this to all previous tokens in the word
          for (size_t i = t;; --i) {
            alignments.back()[i] = currSentenceIdx;

            // Stop if this was the first token or the beginning of the word
            if (i == 0 ||
                !isContinuation(response.target.word(sentenceIdx, i - 1), response.target.word(sentenceIdx, i)))
              break;
          }
        } else {
          alignments.back()[t] = prevSentenceIdx;
        }
      }
    }

    // Always align target end with source end
    alignments.back().push_back(response.source.numWords(sentenceIdx) - 1);

    offset += response.source.numWords(sentenceIdx) + 1;  // +1 for prefix gap
  }
}

}  // namespace marian::bergamot
