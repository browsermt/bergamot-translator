#ifndef SRC_BERGAMOT_TEXT_PROCESSOR_H_
#define SRC_BERGAMOT_TEXT_PROCESSOR_H_

#include "data/types.h"
#include "data/vocab.h"
#include "definitions.h"
#include "annotation.h"

#include "sentence_splitter.h"
#include "vocabs.h"

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
  explicit TextProcessor(Vocabs &vocabs, Ptr<Options>);

  void process(AnnotatedText &source, Segments &segments);

private:
  // Tokenizes an input string, returns Words corresponding. Loads the
  // corresponding byte-ranges into tokenRanges.
  Segment tokenize(const string_view &input,
                   std::vector<string_view> &tokenRanges);

  // Wrap into sentences of at most max_length_break_ tokens and add to source.
  void wrap(Segment &sentence, std::vector<string_view> &tokenRanges,
            Segments &segments, AnnotatedText &source);

  // shorthand, used only in truncate()
  // vocabs_->sources().front() is invoked as we currently only support one source vocab
  const Word sourceEosId() const { return vocabs_.sources().front()->getEosId(); }

  const Vocabs& vocabs_;
  SentenceSplitter sentence_splitter_;
  size_t max_length_break_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_TEXT_PROCESSOR_H_
