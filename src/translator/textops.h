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
  SentenceSplitter sentence_splitter_;
  unsigned int max_input_sentence_tokens_;

public:
  explicit TextProcessor(Ptr<Options>);
  void query_to_segments(const string_view &query, Segments &segments,
                         std::vector<TokenRanges> &sourceRanges);
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_TEXTOPS_H_
