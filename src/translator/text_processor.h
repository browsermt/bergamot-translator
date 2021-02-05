#ifndef SRC_BERGAMOT_TEXT_PROCESSOR_H_
#define SRC_BERGAMOT_TEXT_PROCESSOR_H_

#include "data/types.h"
#include "data/vocab.h"
#include "definitions.h"

#include "sentence_splitter.h"

#include <vector>

namespace marian {
namespace bergamot {

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

#endif // SRC_BERGAMOT_TEXT_PROCESSOR_H_
