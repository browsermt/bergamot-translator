#include "textops.h"
#include "common/timer.h"
#include <pcrecpp.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace marian {
namespace bergamot {

SentenceSplitter::SentenceSplitter(marian::Ptr<marian::Options> options)
    : options_(options) {

  std::string smode_str = options_->get<std::string>("ssplit-mode", "");
  mode_ = string2splitmode(smode_str);
  std::string ssplit_prefix_file =
      options_->get<std::string>("ssplit-prefix-file", "");

  if (ssplit_prefix_file.size()) {
    ssplit_prefix_file = marian::cli::interpolateEnvVars(ssplit_prefix_file);

    LOG(info, "Loading protected prefixes for sentence splitting from {}",
        ssplit_prefix_file);

    ssplit_.load(ssplit_prefix_file);
  } else {
    LOG(warn, "Missing list of protected prefixes for sentence splitting. "
              "Set with --ssplit-prefix-file.");
  }
}

ug::ssplit::SentenceStream
SentenceSplitter::createSentenceStream(const string_view &input) {
  pcrecpp::StringPiece spiece(input.begin(), input.size());
  return std::move(ug::ssplit::SentenceStream(spiece, this->ssplit_, mode_));
}

ug::ssplit::SentenceStream::splitmode
SentenceSplitter::string2splitmode(const std::string &m) {
  typedef ug::ssplit::SentenceStream::splitmode splitmode;
  // @TODO: throw Exception on error
  if (m == "sentence" || m == "Sentence")
    return splitmode::one_sentence_per_line;
  if (m == "paragraph" || m == "Paragraph")
    return splitmode::one_paragraph_per_line;
  if (m != "wrapped_text" && m != "WrappedText" && m != "wrappedText") {
    LOG(warn, "Ignoring unknown text input format specification: {}.", m);
  }
  return splitmode::wrapped_text;
}

Segment TextProcessor::tokenize(const string_view &segment,
                                TokenRanges &tokenRanges) {
  return vocabs_->front()->encodeWithByteRanges(
      segment, tokenRanges, /*addEOS=*/false, /*inference=*/true);
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
                            std::vector<TokenRanges> &sourceRanges) {

  auto sentenceStream = sentence_splitter_.createSentenceStream(query);
  pcrecpp::StringPiece sentenceStringPiece;

  while (sentenceStream >> sentenceStringPiece) {
    string_view sentence(sentenceStringPiece.data(),
                         sentenceStringPiece.size());
    TokenRanges tokenRanges;
    Segment segment = tokenize(sentence, tokenRanges);

    // There are some cases where SentencePiece or vocab returns no words
    // after normalization. 0 prevents any empty entries from being added.
    if (segment.size() > 0) {
      // Truncate segment into max_input_size segments.
      truncate(segment, tokenRanges, segments, sourceRanges);
    }
  }
}

void TextProcessor::truncate(Segment &segment, TokenRanges &tokenRanges,
                             Segments &segments,
                             std::vector<TokenRanges> &sourceRanges) {
  for (int offset = 0; offset < segment.size();
       offset += max_input_sentence_tokens_) {
    auto start = segment.begin() + offset;

    unsigned int left = segment.size() - offset;
    unsigned int diff = std::min(max_input_sentence_tokens_, left);

    segments.emplace_back(start, start + diff);
    segments.back().push_back(sourceEosId());

    auto astart = tokenRanges.begin() + offset;
    sourceRanges.emplace_back(astart, astart + diff);
  }
}

} // namespace bergamot
} // namespace marian
