#include "text_processor.h"
#include "sentence_ranges.h"
#include "data/types.h"
#include "definitions.h"

#include "common/options.h"
#include "data/vocab.h"
#include <vector>

namespace marian {
namespace bergamot {

Segment TextProcessor::tokenize(const string_view &segment,
                                std::vector<string_view> &wordRanges) {
  return vocabs_->front()->encodeWithByteRanges(
      segment, wordRanges, /*addEOS=*/false, /*inference=*/true);
}

TextProcessor::TextProcessor(std::vector<Ptr<Vocab const>> &vocabs,
                             Ptr<Options> options)
    : vocabs_(&vocabs), sentence_splitter_(options) {

  max_input_sentence_tokens_ = options->get<int>("max-input-sentence-tokens");
  max_input_sentence_tokens_ = max_input_sentence_tokens_ - 1;
  ABORT_IF(max_input_sentence_tokens_ < 0,
           "max-input-sentence-tokens cannot be < 0");
}

void TextProcessor::process(const string_view &query, Segments &segments,
                            SentenceRanges &sourceRanges) {

  auto sentenceStream = sentence_splitter_.createSentenceStream(query);
  std::string_view sentenceStringPiece;

  while (sentenceStream >> sentenceStringPiece) {
    marian::string_view sentence(sentenceStringPiece.data(),
                                 sentenceStringPiece.size());

    std::vector<string_view> wordRanges;
    Segment segment = tokenize(sentence, wordRanges);

    // There are some cases where SentencePiece or vocab returns no words
    // after normalization. 0 prevents any empty entries from being added.
    if (segment.size() > 0) {
      // Truncate segment into max_input_size segments.
      truncate(segment, wordRanges, segments, sourceRanges);
    }
  }
}

void TextProcessor::truncate(Segment &segment,
                             std::vector<string_view> &wordRanges,
                             Segments &segments, SentenceRanges &sourceRanges) {
  for (int offset = 0; offset < segment.size();
       offset += max_input_sentence_tokens_) {
    auto start = segment.begin() + offset;

    unsigned int left = segment.size() - offset;
    unsigned int diff = std::min(max_input_sentence_tokens_, left);

    segments.emplace_back(start, start + diff);
    segments.back().push_back(sourceEosId());

    auto astart = wordRanges.begin() + offset;
    sourceRanges.addSentence(astart, astart + diff);
  }
}

} // namespace bergamot
} // namespace marian
