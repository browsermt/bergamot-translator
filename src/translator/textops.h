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
  // A wrapper around @ugermann's ssplit-cpp compiled from several places in
  // mts. Constructed based on options. Used in TextProcessor below to create
  // sentence-streams, which provide access to one sentence from blob of text at
  // a time.
public:
  explicit SentenceSplitter(Ptr<Options> options);
  ug::ssplit::SentenceStream createSentenceStream(string_view const &input);

private:
  ug::ssplit::SentenceSplitter ssplit_;
  Ptr<Options> options_;
  ug::ssplit::SentenceStream::splitmode mode_;
  ug::ssplit::SentenceStream::splitmode string2splitmode(const std::string &m);
};

class TextProcessor {
  // TextProcessor handles loading the sentencepiece vocabulary and also
  // contains an instance of sentence-splitter based on ssplit.
  //
  // Used in Service to convert an incoming blog of text to a vector of
  // sentences (vector of words). In addition, the ByteRanges of the
  // source-tokens in unnormalized text are provided as string_views.
public:
  explicit TextProcessor(std::vector<Ptr<Vocab const>> &vocabs, Ptr<Options>);

  void process(const string_view &query, Segments &segments,
               std::vector<TokenRanges> &sourceRanges);

private:
  // Tokenizes an input string, returns Words corresponding. Loads the
  // corresponding byte-ranges into tokenRanges.
  Segment tokenize(const string_view &input, TokenRanges &tokenRanges);

  // Truncate sentence into max_input_size segments.
  void truncate(Segment &sentence, TokenRanges &tokenRanges, Segments &segments,
                std::vector<TokenRanges> &sourceRanges);

  // shorthand, used only in truncate()
  const Word sourceEosId() const { return vocabs_->front()->getEosId(); }

  std::vector<Ptr<Vocab const>> *vocabs_;
  SentenceSplitter sentence_splitter_;
  unsigned int max_input_sentence_tokens_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_TEXTOPS_H_
