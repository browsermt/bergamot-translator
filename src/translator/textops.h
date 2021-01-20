#ifndef SRC_BERGAMOT_TEXTOPS_H_
#define SRC_BERGAMOT_TEXTOPS_H_

#include "common/definitions.h"
#include "common/logging.h"
#include "common/options.h"
#include "common/types.h" // missing in shortlist.h
#include "common/utils.h"
#include "data/sentencepiece_vocab.h"
#include "data/shortlist.h"
#include "definitions.h"
#include "ssplit/ssplit.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace marian {
namespace bergamot {

class StringViewStream {
private:
  string_view text_;
  string_view::iterator current_;

public:
  StringViewStream(const string_view &text) : text_(text) {
    current_ = text_.begin();
  }

  bool operator>>(string_view &sentence_view) {
    // Skip to the next non-newline; whitespaces, anything else are okay.
    while (current_ != text_.end() &&
           (*current_ == '\n' || *current_ == ' ' || *current_ == '\t')) {
      ++current_;
    }

    string_view::iterator p = current_;
    while (p != text_.end() && *p != '\n') {
      ++p;
    }

    if (p == current_)
      return false;

    sentence_view = string_view(current_, p - current_);
    current_ = p;
    return true;
  };
};

class SentenceSplitter {
public:
  explicit SentenceSplitter(Ptr<Options> options);
  ug::ssplit::SentenceStream createSentenceStream(string_view const &input);

private:
  ug::ssplit::SentenceSplitter ssplit_;
  Ptr<Options> options_;
  ug::ssplit::SentenceStream::splitmode mode_;
  ug::ssplit::SentenceStream::splitmode string2splitmode(const std::string &m);
};

class LineSplitter {
public:
  explicit LineSplitter(Ptr<Options> options){
      // Do nothing.
  };
  StringViewStream createSentenceStream(string_view const &input) {
    return std::move(StringViewStream(input));
  }
};

class Tokenizer {
private:
  std::vector<Ptr<Vocab const>> vocabs_;
  bool inference_;
  bool addEOS_;

public:
  explicit Tokenizer(Ptr<Options>);
  Segment tokenize(const string_view &input, TokenRanges &tokenRanges);
  Word sourceEosId() { return vocabs_.front()->getEosId(); };
};

class TextProcessor {
private:
  Tokenizer tokenizer_;
  LineSplitter sentence_splitter_;
  unsigned int max_input_sentence_tokens_;

public:
  explicit TextProcessor(Ptr<Options>);
  void query_to_segments(const string_view &query, Segments &segments,
                         std::vector<TokenRanges> &sourceRanges);
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_TEXTOPS_H_
