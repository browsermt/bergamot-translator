#include "text_processor.h"

#include <vector>

#include "annotation.h"
#include "common/cli_helper.h"
#include "common/options.h"
#include "data/types.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

namespace {
ug::ssplit::SentenceStream::splitmode string2splitmode(const std::string &m) {
  typedef ug::ssplit::SentenceStream::splitmode splitmode;
  // @TODO: throw Exception on error
  if (m == "sentence" || m == "Sentence") return splitmode::one_sentence_per_line;
  if (m == "paragraph" || m == "Paragraph") return splitmode::one_paragraph_per_line;
  if (m != "wrapped_text" && m != "WrappedText" && m != "wrappedText") {
    LOG(warn, "Ignoring unknown text input format specification: {}.", m);
  }
  return splitmode::wrapped_text;
}

ug::ssplit::SentenceSplitter loadSplitter(const std::string &ssplit_prefix_file) {
  // Temporarily supports empty, will be removed when mozilla passes ssplit_prefix_file
  ug::ssplit::SentenceSplitter splitter;
  if (ssplit_prefix_file.size()) {
    std::string interp_ssplit_prefix_file = marian::cli::interpolateEnvVars(ssplit_prefix_file);
    LOG(info, "Loading protected prefixes for sentence splitting from {}", interp_ssplit_prefix_file);
    splitter.load(interp_ssplit_prefix_file);
  } else {
    LOG(warn,
        "Missing list of protected prefixes for sentence splitting. "
        "Set with --ssplit-prefix-file.");
  }
  return splitter;
}

ug::ssplit::SentenceSplitter loadSplitter(const AlignedMemory &memory) {
  // Temporarily supports empty, will be removed when mozilla passes memory
  ug::ssplit::SentenceSplitter splitter;
  if (memory.size()) {
    std::string_view serialized(memory.begin(), memory.size());
    splitter.loadFromSerialized(serialized);
  }
  return splitter;
}

}  // namespace

Segment TextProcessor::tokenize(const string_view &segment, std::vector<string_view> &wordRanges) {
  // vocabs_->sources().front() is invoked as we currently only support one source vocab
  return vocabs_.sources().front()->encodeWithByteRanges(segment, wordRanges, /*addEOS=*/false, /*inference=*/true);
}

TextProcessor::TextProcessor(Ptr<Options> options, const Vocabs &vocabs, const std::string &ssplit_prefix_file)
    : vocabs_(vocabs), ssplit_(loadSplitter(ssplit_prefix_file)) {
  parseCommonOptions(options);
}

TextProcessor::TextProcessor(Ptr<Options> options, const Vocabs &vocabs, const AlignedMemory &memory)
    : vocabs_(vocabs), ssplit_(loadSplitter(memory)) {
  parseCommonOptions(options);
}

void TextProcessor::parseCommonOptions(Ptr<Options> options) {
  maxLengthBreak_ = options->get<int>("max-length-break");
  maxLengthBreak_ = maxLengthBreak_ - 1;  // Accounting for EOS
  ABORT_IF(maxLengthBreak_ < 0, "max-length-break cannot be < 0");
  ssplitMode_ = string2splitmode(options->get<std::string>("ssplit-mode", ""));
}

void TextProcessor::process(std::string &&input, AnnotatedText &source, Segments &segments) {
  source = std::move(AnnotatedText(std::move(input)));
  std::string_view input_converted(source.text.data(), source.text.size());
  auto sentenceStream = ug::ssplit::SentenceStream(input_converted, ssplit_, ssplitMode_);

  std::string_view sentenceStringPiece;

  while (sentenceStream >> sentenceStringPiece) {
    marian::string_view sentence(sentenceStringPiece.data(), sentenceStringPiece.size());

    std::vector<string_view> wordRanges;
    Segment segment = tokenize(sentence, wordRanges);

    // There are some cases where SentencePiece or vocab returns no words
    // after normalization. 0 prevents any empty entries from being added.
    if (segment.size() > 0) {
      // Wrap segment into sentences of at most maxLengthBreak_ tokens and
      // tell source about them.
      wrap(segment, wordRanges, segments, source);
    }
  }
}

void TextProcessor::wrap(Segment &segment, std::vector<string_view> &wordRanges, Segments &segments,
                         AnnotatedText &source) {
  Word sourceEosId = vocabs_.sources().front()->getEosId();
  for (size_t offset = 0; offset < segment.size(); offset += maxLengthBreak_) {
    auto start = segment.begin() + offset;

    size_t left = segment.size() - offset;
    size_t diff = std::min(maxLengthBreak_, left);

    segments.emplace_back(start, start + diff);
    segments.back().push_back(sourceEosId);

    auto astart = wordRanges.begin() + offset;
    // diff > 0
    source.recordExistingSentence(astart, astart + diff, astart->data());
  }
}

}  // namespace bergamot
}  // namespace marian
