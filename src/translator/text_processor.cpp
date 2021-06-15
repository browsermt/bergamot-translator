#include "text_processor.h"

#include <vector>

#include "annotation.h"
#include "common/options.h"
#include "data/types.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

Segment TextProcessor::tokenize(const string_view &segment, std::vector<string_view> &wordRanges) {
  // vocabs_->sources().front() is invoked as we currently only support one source vocab
  return vocabs_.sources().front()->encodeWithByteRanges(segment, wordRanges, /*addEOS=*/false, /*inference=*/true);
}

TextProcessor::TextProcessor(Vocabs &vocabs, Ptr<Options> options) : vocabs_(vocabs), sentence_splitter_(options) {
  max_length_break_ = options->get<int>("max-length-break");
  max_length_break_ = max_length_break_ - 1;
  ABORT_IF(max_length_break_ < 0, "max-length-break cannot be < 0");
}

void TextProcessor::process(AnnotatedText &source, Segments &segments) {
  string_view query = string_view(source.text);
  auto sentenceStream = sentence_splitter_.createSentenceStream(query);
  std::string_view sentenceStringPiece;

  while (sentenceStream >> sentenceStringPiece) {
    marian::string_view sentence(sentenceStringPiece.data(), sentenceStringPiece.size());

    std::vector<string_view> wordRanges;
    Segment segment = tokenize(sentence, wordRanges);

    // There are some cases where SentencePiece or vocab returns no words
    // after normalization. 0 prevents any empty entries from being added.
    if (segment.size() > 0) {
      // Wrap segment into sentences of at most max_length_break_ tokens and
      // tell source about them.
      wrap(segment, wordRanges, segments, source);
    }
  }
}

void TextProcessor::wrap(Segment &segment, std::vector<string_view> &wordRanges, Segments &segments,
                         AnnotatedText &source) {
  for (size_t offset = 0; offset < segment.size(); offset += max_length_break_) {
    auto start = segment.begin() + offset;

    size_t left = segment.size() - offset;
    size_t diff = std::min(max_length_break_, left);

    segments.emplace_back(start, start + diff);
    segments.back().push_back(sourceEosId());

    auto astart = wordRanges.begin() + offset;

    // Construct a part vector of string_view representing wrapped segment, use the last string_view to create an EOS
    // string_view manually.
    std::vector<string_view> partWordRanges(astart, astart + diff);
    string_view &last = partWordRanges.back();
    const char *end = last.data() + last.size();
    partWordRanges.emplace_back(end, 0);
    // diff > 0
    source.recordExistingSentence(partWordRanges.begin(), partWordRanges.end(), astart->data());
  }
}

}  // namespace bergamot
}  // namespace marian
