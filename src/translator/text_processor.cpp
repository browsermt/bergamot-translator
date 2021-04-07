#include "text_processor.h"
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
  std::vector<std::pair<size_t, size_t>> faults;
  for (size_t i = 1; i < wordRanges.size(); i++) {
    string_view first = wordRanges[i - 1];
    string_view second = wordRanges[i];
    bool assertion = (first.data() + first.size()) == second.data();
    if (assertion == false) {
      faults.emplace_back(i - 1, i);
    }
    // ABORT_IF(assertion == false,
    //          "Something's wrong, we don't have consecutive words");
  }
  if (faults.empty()) {
    LOG(info, "All okay at line: {}", std::string(segment));
  } else {
    LOG(info, "Something's wrong in line(length={}): {}", segment.size(),
        std::string(segment));
    std::stringstream faultsStream;
    bool first = true;
    for (auto &p : faults) {
      if (not first) {
        first = false;
      } else {
        faultsStream << " ";
      }
      size_t i = p.first, j = p.second;

      auto rebase = [&segment](const char *data) {
        size_t diff = data - segment.data();
        return diff;
      };

      faultsStream << "(" << rebase(wordRanges[i].data()) << ","
                   << rebase(wordRanges[i].data() + wordRanges[i].size())
                   << ")";
      faultsStream << "-" << rebase(wordRanges[j].data());
    }
    LOG(info, "At: {} ", faultsStream.str());
  }

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

  std::vector<string_view> wordRanges;
  while (sentenceStream >> sentenceStringPiece) {
    marian::string_view sentence(sentenceStringPiece.data(),
                                 sentenceStringPiece.size());

    wordRanges.clear();
    Segment segment = tokenize(sentence, wordRanges);

    // There are some cases where SentencePiece or vocab returns no words
    // after normalization. 0 prevents any empty entries from being added.
    if (segment.size() > 0) {
      // Truncate segment into max_input_size segments.
      truncate(segment, wordRanges, segments, source);
    }
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
