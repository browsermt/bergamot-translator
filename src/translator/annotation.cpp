#include "annotation.h"

#include <cassert>

namespace marian {
namespace bergamot {

AnnotatedText::AnnotatedText(std::string &&t) : text(std::move(t)) {
  // Treat the entire text as a gap that recordExistingSentence will break.
  annotation.token_begin_.back() = text.size();
}

void AnnotatedText::appendSentence(string_view prefix, std::vector<string_view>::iterator begin,
                                   std::vector<string_view>::iterator end) {
  assert(annotation.token_begin_.back() == text.size());

  // prefix is just end of the previous one.
  appendEndingWhitespace(prefix);

  // Appending sentence text.
  std::size_t offset = text.size();
  for (std::vector<string_view>::iterator token = begin; token != end; ++token) {
    offset += token->size();
    annotation.token_begin_.push_back(offset);
  }
  if (begin != end) {
    text.append(begin->data(), (end - 1)->data() + (end - 1)->size());
    assert(offset == text.size());  // Tokens should be contiguous.
  }

  // Add the gap after the sentence.  This is empty for now, but will be
  // extended with appendEndingWhitespace or another appendSentence.
  annotation.gap_.push_back(annotation.token_begin_.size() - 1);
  annotation.token_begin_.push_back(offset);
}

void AnnotatedText::appendEndingWhitespace(string_view whitespace) {
  text.append(whitespace.data(), whitespace.size());
  annotation.token_begin_.back() = text.size();
}

void AnnotatedText::recordExistingSentence(std::vector<string_view>::iterator begin,
                                           std::vector<string_view>::iterator end, const char *sentence_begin) {
  assert(sentence_begin >= text.data());
  assert(sentence_begin <= text.data() + text.size());
  assert(begin == end || sentence_begin == begin->data());
  assert(!annotation.token_begin_.empty());
  assert(annotation.token_begin_.back() == text.size());
  // Clip off size token ending.
  annotation.token_begin_.pop_back();
  for (std::vector<string_view>::iterator i = begin; i != end; ++i) {
    assert(i->data() >= text.data());                                  // In range.
    assert(i->data() + i->size() <= text.data() + text.size());        // In range
    assert(i + 1 == end || i->data() + i->size() == (i + 1)->data());  // Contiguous
    annotation.token_begin_.push_back(i->data() - text.data());
  }
  // Gap token after sentence.
  annotation.gap_.push_back(annotation.token_begin_.size());
  if (begin != end) {
    annotation.token_begin_.push_back((end - 1)->data() + (end - 1)->size() - text.data());
  } else {
    // empty sentence.
    annotation.token_begin_.push_back(sentence_begin - text.data());
  }
  // Add back size token ending.
  annotation.token_begin_.push_back(text.size());
}

}  // namespace bergamot
}  // namespace marian
