#include "html.h"

#include "response.h"
#include "xh_scanner.h"

namespace {
using marian::string_view;
using marian::bergamot::AnnotatedText;
using marian::bergamot::ByteRange;
using marian::bergamot::HTML;
using marian::bergamot::Response;

void encodeEntities(string_view const &input, std::string &output) {
  output.clear();
  output.reserve(input.size());  // assumes there are no entities in most cases

  for (auto it = input.begin(); it != input.end(); ++it) {
    switch (*it) {
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
        output.push_back(*it);
        break;
    }
  }
}

size_t countPrefixWhitespaces(string_view const &input) {
  size_t size = 0;
  while (size < input.size() && std::isspace(input[size])) ++size;
  return size;
}

// Formatters used for exception messages combined with format()
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

std::ostream &operator<<(std::ostream &out, HTML::Taint const &tags) {
  for (auto it = tags.begin(); it != tags.end(); ++it) {
    if (it != tags.begin()) out << ' ';
    out << *it;
  }
  return out;
}

// Very simple replacement for std::format introduced in C++20
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

// Syntactic sugar around rbegin() and rend() that allows me to write
// `for (auto &&item : reversed(container))` instead of the needlessly verbose
// `for (auto it = container.rbegin(); it != container.rend(); ++it)`
template <typename T>
class reversed {
 public:
  typedef typename T::const_reverse_iterator iterator;
  explicit reversed(T const &container) : container_(container){};
  iterator begin() const { return container_.rbegin(); }
  iterator end() const { return container_.rend(); }

 private:
  T const &container_;
};

bool contains(std::unordered_set<std::string> const &set, std::string const &name) {
  return set.find(name) != set.end();
}

void diffTags(HTML::Taint const &prev, HTML::Taint const &curr, HTML::Taint &opening, HTML::Taint &closing) {
  opening.clear();
  closing.clear();

  size_t i = 0;

  // Find first difference
  for (; i < prev.size(); ++i)
    if (i >= curr.size() || prev[i] != curr[i]) break;

  // Only nodes of type ELEMENT can have children and thus would need a closing tag.
  std::copy_if(prev.begin() + i, prev.end(), std::back_inserter(closing),
               [&](HTML::Tag *tag) { return tag->type == HTML::Tag::ELEMENT; });

  opening.insert(opening.end(), curr.begin() + i, curr.end());
}

bool intersects(ByteRange const &range, HTML::Span const &span) {
  return range.begin <= span.end && range.end >= span.begin;
};

bool containsTag(HTML::Taint const &stack, HTML::Tag const *tag) {
  return std::find(stack.rbegin(), stack.rend(), tag) != stack.rend();
}

template <typename Fun>
AnnotatedText apply(AnnotatedText const &in, Fun fun) {
  AnnotatedText out;

  for (size_t sentenceIdx = 0; sentenceIdx < in.numSentences(); ++sentenceIdx) {
    std::string sentence;
    std::vector<ByteRange> tokens;

    std::string prefix = fun(in.annotation.gap(sentenceIdx), in.gap(sentenceIdx), false);

    for (size_t wordIdx = 0; wordIdx < in.numWords(sentenceIdx); ++wordIdx) {
      std::string token = fun(in.wordAsByteRange(sentenceIdx, wordIdx), in.word(sentenceIdx, wordIdx), false);
      tokens.push_back(ByteRange{sentence.size(), sentence.size() + token.size()});
      sentence += token;
    }

    // Convert our ByteRanges to string_views since that's what appendSentence
    // expects
    // TODO: extend AnnotatedText::appendSentence to accept str + ByteRanges
    // directly
    std::vector<string_view> views(tokens.size());
    std::transform(tokens.begin(), tokens.end(), views.begin(),
                   [&](ByteRange const &range) { return string_view(sentence.data() + range.begin, range.size()); });

    out.appendSentence(prefix, views.begin(), views.end());
  }

  out.appendEndingWhitespace(fun(in.annotation.gap(in.numSentences()), in.gap(in.numSentences()), true));

  return out;
}

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

// Little helper class to append HTML to a token
class TokenFormatter {
 public:
  explicit TokenFormatter(string_view token)
      : html_(), offset_(0), whitespaceOffset_(0), whitespaceSize_(countPrefixWhitespaces(token)), closeLeft_(true) {
    // Do encoding of any entities that popped up in the translation
    encodeEntities(token, html_);
  }

  std::string &&html() { return std::move(html_); }

  // Append the markup necessary for moving from `prev` set of tags to `curr`.
  void append(HTML::Taint const &prev, HTML::Taint const &curr) {
    HTML::Taint opening, closing;

    diffTags(prev, curr, opening, closing);

    for (HTML::Tag const *tag : reversed(closing)) {
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

size_t debugCountTokens(AnnotatedText const &text) {
  size_t tokens = 1;  // for the ending gap
  for (size_t sentenceIdx = 0; sentenceIdx < text.numSentences(); ++sentenceIdx) {
    tokens += 1 + text.numWords(sentenceIdx);  // pre-sentence prefix/gap + each word
  }
  return tokens;
}

}  // namespace

namespace marian::bergamot {

HTML::HTML(Options &&options) : options_(std::move(options)) {}

std::optional<HTML::Error> HTML::parse(std::string &source) {
  // Clean out any possible previous parse state
  spans_.clear();
  pool_.clear();

  std::string original = std::move(source);
  markup::instream in(original.data(), original.data() + original.size());
  markup::Scanner scanner(in);
  source.clear();  // source is moved out of, so should be clear anyway

  Tag *tag;
  Taint stack;
  bool addSentenceBreak = false;
  bool addSpace = false;
  spans_.push_back(Span{0, 0, {}});

  // If we encounter an error, clear our internal state so a call to restore()
  // doesn't do anything half-assed. Also undo any changes to `source` which
  // we got by reference. Finally format an error message.
  auto fail = [&](auto &&...args) {
    source = std::move(original);
    spans_.clear();
    pool_.clear();
    return Error{format(std::forward<decltype(args)>(args)...)};
  };

  bool stop = false;
  while (!stop) {
    switch (scanner.next()) {
      case markup::Scanner::TT_ERROR:
        return fail("HTML parse error");

      case markup::Scanner::TT_EOF:
        stop = true;
        break;

      case markup::Scanner::TT_TEXT: {
        // If the previous segment was the open or close tag of a block element
        // we treat the text after it as a new sentence.
        if (addSentenceBreak) {
          if (!(source.empty() || (source.size() > 2 && source.substr(source.size() - 2) == ""))) {
            stack.push_back(makeTag({Tag::WHITESPACE}));
            // Important: span->size() == 0 to make it behave as a void element.
            // Also important: position before the \n\n tokens, not after, to
            // make it easier to remove them later through apply().
            spans_.push_back(Span{source.size(), source.size(), stack});
            source.append("\n\n");  // TODO assumes ssplit-mode = wrapped_text
            stack.pop_back();
          }
          addSentenceBreak = false;
        }

        // If the previous segment was an open or close tag, it might be best
        // to add a space to make sure we don't append to the previous word.
        if (addSpace) {
          if (options_.substituteInlineTagsWithSpaces && !source.empty() && !std::isspace(source.back()) &&
              !std::isspace(scanner.value()[0])) {
            source.push_back(' ');
          }
          addSpace = false;
        }

        auto begin = source.size();
        source.append(scanner.value());
        spans_.push_back(Span{begin, source.size(), stack});
      } break;

      case markup::Scanner::TT_TAG_START: {
        std::string name(scanner.tag());

        // Tag *tag is used by attribute parsing
        tag = makeTag({contains(options_.voidTags, name) ? Tag::VOID_ELEMENT : Tag::ELEMENT, std::move(name)});

        stack.push_back(tag);

        // Empty elements (e.g. <img>) are not applicable to a span of text
        // so instead we "apply" them to an empty span in between, and then
        // immediately remove them again from the stack.
        if (tag->type == Tag::VOID_ELEMENT) {
          spans_.push_back(Span{source.size(), source.size(), stack});
          stack.pop_back();
        }

        // Treat non-inline HTML tags as spaces that break up words.
        if (!contains(options_.inlineTags, tag->name)) {
          addSentenceBreak = true;
        } else {
          addSpace = true;
        }
      } break;

      case markup::Scanner::TT_TAG_END:
        // If this is the closing bit of a void tag, i.e. triggered by the "/>"
        // bit of "<img/>", then completely ignore it.
        if (contains(options_.voidTags, std::string(scanner.tag()))) break;

        if (stack.empty()) return fail("Encountered more closing tags ({}) than opening tags", scanner.tag());

        if (stack.back()->name != scanner.tag())
          return fail("Encountered unexpected closing tag </{}>, stack is {}", scanner.tag(), stack);

        // What to do with "<u></u>" case, where tag is immediately closed
        // so it never makes it into the taint of any of the spans? This adds
        // an empty span so it still gets recorded in spans_.
        if (spans_.empty() || !containsTag(spans_.back().tags, stack.back()))
          spans_.push_back(Span{source.size(), source.size(), stack});

        stack.pop_back();

        // Add space if necessary
        if (!contains(options_.inlineTags, std::string(scanner.tag()))) {
          addSentenceBreak = true;
        } else {
          addSpace = true;
        }
        break;

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
        return fail("Unsupported scanner token type");
    }
  }

  if (!stack.empty()) return fail("Not all tags were closed: {}", stack);

  // Add a trailing span (that's empty) to signify all closed tags.
  spans_.emplace_back(Span{source.size(), source.size(), stack});

  return std::nullopt;
}

std::optional<HTML::Error> HTML::restore(Response &response) {
  // restore() is a no-op if parse() wasn't called or wasn't successful. We have
  // no state to restore in that case.
  if (spans_.empty()) return std::nullopt;

  // We need alignment info to transfer the HTML tags from the input to the
  // translation. If those are not available, no HTML in translations for you.
  if (!hasAlignments(response))
    return Error{"Response object does not contain alignments. TranslationModel or ResponseOptions is misconfigured?"};

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
  hardAlignments(response, alignments);

  std::vector<SpanIterator> targetTokenSpans;
  copyTaint(response, alignments, sourceTokenSpans, targetTokenSpans);
  assert(targetTokenSpans.size() == debugCountTokens(response.target));

  AnnotatedText target = restoreTarget(response.target, targetTokenSpans);

  response.source = source;
  response.target = target;

  return std::nullopt;
}

AnnotatedText HTML::restoreSource(AnnotatedText const &in, std::vector<SpanIterator> &sourceTokenSpans) {
  auto spanIt = spans_.begin();
  auto prevIt = spans_.begin();  // safe because first span is always empty span, and
                                 // and the while-loop below will do the rest
  assert(prevIt == spans_.end() || prevIt->tags.empty());

  return apply(in, [&](ByteRange range, string_view token, bool last) {
    TokenFormatter formatter(token);

    // Potential issue: spans and tokens can intersect, e.g.
    //
    //    text  <p> h <u> e </u> ll o </p>
    //   spans     |1|   |2|    |3333| (so only 2 is tainted with <p><u>, others only <p>)
    //  tokens     |111111111111111|2|
    //
    // Now 1 covers span 1 to 3, so what taint should it get? Just <p>, or <p><u>?
    // Note: only relevant if isBlockElement is used. If we just insert spaces
    // around all elements, every segment of `hello` will be a token.

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
    sourceTokenSpans.push_back(prevIt);

    return std::move(formatter.html());
  });
}

AnnotatedText HTML::restoreTarget(AnnotatedText const &in, std::vector<SpanIterator> const &targetTokenSpans) {
  auto prevSpan = spans_.cbegin();
  auto targetSpanIt = targetTokenSpans.begin();

  AnnotatedText out = apply(in, [&](ByteRange range, string_view token, bool last) {
    TokenFormatter formatter(token);

    // First we scan through spans_ to catch up to the span assigned to this
    // token. We're only interested in empty spans (empty and void elements)
    for (auto span_it = prevSpan; span_it < *targetSpanIt; span_it++) {
      // We're only interested in empty spans or spans that would otherwise get
      // lost because they didn't align with anything between the spans in
      // targetSpanIt
      // TODO That std::find makes this O(N*N) NOT GOOD NOT GOOD
      if (span_it->size() != 0 &&
          std::find(targetTokenSpans.begin(), targetTokenSpans.end(), span_it) != targetTokenSpans.end())
        continue;

      formatter.append(prevSpan->tags, span_it->tags);

      // Note: here, not in 3rd part of for-statement because we don't want to
      // set prevSpan if the continue clause at the beginning of this for-loop
      // was hit.
      prevSpan = span_it;
    }

    // Now do the same thing but for our target set of tags. Note that we cannot
    // combine this in the for-loop above (i.e. `span_it <= *targetSpanIt`)
    // because there is no guarantee that the order in `targetTokenSpans` is
    // the same as that of `spans`.
    formatter.append(prevSpan->tags, (*targetSpanIt)->tags);

    // If this is the last token of the response, close all open tags.
    if (last) {
      // Note: this assert is true due to our current implementation of
      // HardAlignments() that always matches the last token of the input with
      // the last token of the output. But lets assume someone someday changes
      // HardAlignments(), and then this for-loop will be necessary.
      // assert((*targetSpanIt)->tags.empty());
      formatter.append((*targetSpanIt)->tags, HTML::Taint());
    }

    prevSpan = *targetSpanIt;
    ++targetSpanIt;

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

void HTML::copyTaint(Response const &response, std::vector<std::vector<size_t>> const &alignments,
                     std::vector<SpanIterator> const &sourceTokenSpans, std::vector<SpanIterator> &targetTokenSpans) {
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

// Reports if token `str` is likely to be a continuation of a word. This is used
// to determine whether we should share the markup, or whether we should see
// this token as a fresh start. This implementation will treat "hello[world]"
// as 4 words, assuming its tokenised as something like `h ell o [ wor ld ]`.
bool HTML::isContinuation(string_view prev, string_view str) {
  if (options_.continuationDelimiters.empty()) return false;
  if (prev.empty() || str.empty()) return false;
  return options_.continuationDelimiters.find(str[0]) == std::string::npos &&
         options_.continuationDelimiters.find(prev.back()) == std::string::npos;
}

void HTML::hardAlignments(Response const &response, std::vector<std::vector<size_t>> &alignments) {
  // For each sentence...
  for (size_t sentenceIdx = 0; sentenceIdx < response.target.numSentences(); ++sentenceIdx) {
    alignments.emplace_back();

    // Hard-align: find for each target token the most prevalent source token
    // Note: only search from 0 to N-1 because token N is end-of-sentence token
    // that can only align with the end-of-sentence token of the target
    for (size_t t = 0; t + 1 < response.target.numWords(sentenceIdx); ++t) {
      size_t maxS = 0;
      for (size_t s = 1; s + 1 < response.source.numWords(sentenceIdx); ++s) {
        if (response.alignments[sentenceIdx][t][s] > response.alignments[sentenceIdx][t][maxS]) {
          maxS = s;
        }
      }

      alignments.back().push_back(maxS);
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

        if (currScore >= prevScore) {
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
  }
}

}  // namespace marian::bergamot
