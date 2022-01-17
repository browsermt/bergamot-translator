#ifndef SRC_BERGAMOT_TEXT_PROCESSOR_H_
#define SRC_BERGAMOT_TEXT_PROCESSOR_H_

#include <vector>

#include "aligned.h"
#include "annotation.h"
#include "data/types.h"
#include "data/vocab.h"
#include "definitions.h"
#include "ssplit.h"
#include "vocabs.h"

namespace marian {
namespace bergamot {

class TextProcessor {
  /// TextProcessor handles loading the sentencepiece vocabulary and also
  /// contains an instance of sentence-splitter based on ssplit.
  ///
  /// Used in Service to convert an incoming blob of text to a vector of
  /// sentences (vector of words). In addition, the ByteRanges of the
  /// source-tokens in unnormalized text are provided as string_views.
 public:
  // There are two ways to construct text-processor, different in a file-system
  // based prefix file load and a memory based prefix file store. @jerinphilip
  // is not doing magic inference inside to determine file-based or memory
  // based on one being empty or not.

  /// Construct TextProcessor from options, vocabs and prefix-file.
  /// @param [in] options: expected to contain `max-length-break`, `ssplit-mode`.
  /// @param [in] vocabs: Vocabularies used to process text into sentences to marian::Words and corresponding ByteRange
  /// information in AnnotatedText.
  /// @param [in] ssplit_prefix_file: Path to ssplit-prefix file compatible with moses-tokenizer.
  TextProcessor(Ptr<Options>, const Vocabs &vocabs, const std::string &ssplit_prefix_file);

  /// Construct TextProcessor from options, vocabs and prefix-file supplied as a bytearray. For other parameters, see
  /// the path based constructor.
  /// Note: This falls back to string based loads if memory is null, this behaviour will be deprecated in the future.
  ///
  /// @param [in] memory: ssplit-prefix-file contents in memory, passed as a bytearray.
  TextProcessor(Ptr<Options>, const Vocabs &vocabs, const AlignedMemory &memory);

  /// Wrap into sentences of at most maxLengthBreak_ tokens and add to source.
  /// @param [in] blob: Input blob, will be bound to source and annotations on it stored.
  /// @param [out] source: AnnotatedText instance holding input and annotations of sentences and pieces
  /// @param [out] segments: marian::Word equivalents of the sentences processed and stored in AnnotatedText for
  /// consumption of marian translation pipeline.

  void process(std::string &&blob, AnnotatedText &source, Segments &segments) const;

  void processFromAnnotation(AnnotatedText &source, Segments &segments) const;

 private:
  void parseCommonOptions(Ptr<Options> options);

  /// Tokenizes an input string, returns Words corresponding. Loads the
  /// corresponding byte-ranges into tokenRanges.
  Segment tokenize(const string_view &input, std::vector<string_view> &tokenRanges) const;

  /// Wrap into sentences of at most maxLengthBreak_ tokens and add to source.
  void wrap(Segment &sentence, std::vector<string_view> &tokenRanges, Segments &segments, AnnotatedText &source) const;

  const Vocabs &vocabs_;   ///< Vocabularies used to tokenize a sentence
  size_t maxLengthBreak_;  ///< Parameter used to wrap sentences to a maximum number of tokens

  /// SentenceSplitter compatible with moses sentence-splitter
  ug::ssplit::SentenceSplitter ssplit_;

  /// Mode of splitting, can be line ('\n') based, paragraph based, also supports a wrapped mode.
  ug::ssplit::SentenceStream::splitmode ssplitMode_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_TEXT_PROCESSOR_H_
