#include "text_processor.h"
#include "bergamot_utils.h"
#include "data/types.h"
#include "definitions.h"
#include "sentence_ranges.h"

#include "common/options.h"
#include "data/vocab.h"
#include <vector>

namespace marian {
namespace bergamot {

Segment TextProcessor::tokenize(const string_view &segment,
                                std::vector<string_view> &wordRanges) {
  Segment words = vocabs_->front()->encodeWithByteRanges(
      segment, wordRanges, /*addEOS=*/false, /*inference=*/true);
  debugContiguity("source-spiece", segment, wordRanges);
  return words;
}

TextProcessor::TextProcessor(std::vector<Ptr<Vocab const>> &vocabs,
                             Ptr<Options> options)
    : vocabs_(&vocabs), sentence_splitter_(options) {

  max_length_break_ = options->get<int>("max-length-break");
  max_length_break_ = max_length_break_ - 1;
  ABORT_IF(max_length_break_ < 0, "max-length-break cannot be < 0");
}

void TextProcessor::process(AnnotatedText &source, Segments &segments) {

  string_view query = string_view(source.text);
  auto sentenceStream = sentence_splitter_.createSentenceStream(query);
  std::string_view sentenceStringPiece;
  const char *data = &(source.text[0]);
  marian::string_view previousStringPiece(data, 0);

  std::vector<string_view> wordRanges;
  size_t sentenceNumber{0};
  while (sentenceStream >> sentenceStringPiece) {
    marian::string_view sentence(sentenceStringPiece.data(),
                                 sentenceStringPiece.size());

    bool assertion = (previousStringPiece.data() +
                      previousStringPiece.size()) == sentence.data();

    auto rebase = [data](const char *sdata) {
      size_t diff = sdata - data;
      return diff;
    };

    if (assertion == false) {
      size_t previousEnd =
          rebase(previousStringPiece.data() + previousStringPiece.size());
      size_t currentBegin = rebase(sentence.data());
      LOG(info, "[fail] at sentence {}, Byte(diff={}, {}, {}): {}",
          sentenceNumber, currentBegin - previousEnd, previousEnd, currentBegin,
          sentence);
    }

    wordRanges.clear();
    Segment segment = tokenize(sentence, wordRanges);

    // There are some cases where SentencePiece or vocab returns no words
    // after normalization. 0 prevents any empty entries from being added.
    if (segment.size() > 0) {
      // Truncate segment into max_input_size segments.
      truncate(segment, wordRanges, segments, source);
    }

    previousStringPiece = sentence;
    ++sentenceNumber;
  }
}

void TextProcessor::truncate(Segment &segment,
                             std::vector<string_view> &wordRanges,
                             Segments &segments, AnnotatedText &source) {
  for (size_t offset = 0; offset < segment.size();
       offset += max_length_break_) {
    auto start = segment.begin() + offset;

    size_t left = segment.size() - offset;
    size_t diff = std::min(max_length_break_, left);

    segments.emplace_back(start, start + diff);
    segments.back().push_back(sourceEosId());

    auto astart = wordRanges.begin() + offset;
    source.addSentence(astart, astart + diff);
  }
}

} // namespace bergamot
} // namespace marian
