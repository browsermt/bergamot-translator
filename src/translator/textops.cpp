#include "textops.h"
#include "common/timer.h"
#include "utils.h"
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

Tokenizer::Tokenizer(Ptr<Options> options) : inference_(true), addEOS_(false) {
  vocabs_ = loadVocabularies(options);
}

Segment Tokenizer::tokenize(const string_view &snt, TokenRanges &tokenRanges) {
  // TODO(jerin): Bunch of hardcode here, 1, 0, need to get rid off somehow.
  return vocabs_[0]->encodePreservingSource(snt, tokenRanges, addEOS_,
                                            inference_);
}

TextProcessor::TextProcessor(Ptr<Options> options)
    : tokenizer_(options), sentence_splitter_(options) {
  max_input_sentence_tokens_ = options->get<int>("max-input-sentence-tokens");
  max_input_sentence_tokens_ =
      max_input_sentence_tokens_ - 1; // Account for EOS
  // Dirty assert, should do at configparse
  assert(max_input_sentence_tokens_ > 0);
}

void TextProcessor::query_to_segments(const string_view &query,
                                      Segments &segments,
                                      std::vector<TokenRanges> &sourceRanges) {
  auto buf = sentence_splitter_.createSentenceStream(query);
  // pcrecpp::StringPiece snt;
  string_view snt;

  int sentencesProcessed{0};

  while (buf >> snt) {
    // LOG(info, "SNT: {}", snt);
    string_view snt_string_view(snt.data(), snt.size());
    TokenRanges snt_alignment;
    timer::Timer spiece_timer;
    Segment tokenized_sentence =
        tokenizer_.tokenize(snt_string_view, snt_alignment);

    // LOG(info, "Tokenization took {:.5f} seconds", spiece_timer.elapsed());
    if (tokenized_sentence.size() > 0) {
      if (tokenized_sentence.size() > max_input_sentence_tokens_) {
        int offset;
        for (offset = 0;
             offset + max_input_sentence_tokens_ < tokenized_sentence.size();
             offset += max_input_sentence_tokens_) {
          auto start = tokenized_sentence.begin() + offset;
          Segment segment(start, start + max_input_sentence_tokens_);
          segment.push_back(tokenizer_.sourceEosId());
          segments.push_back(segment);

          auto astart = snt_alignment.begin() + offset;
          TokenRanges segment_alignment(astart,
                                        astart + max_input_sentence_tokens_);
          sourceRanges.push_back(segment_alignment);
        }

        if (offset < max_input_sentence_tokens_) {
          auto start = tokenized_sentence.begin() + offset;
          Segment segment(start, tokenized_sentence.end());
          segment.push_back(tokenizer_.sourceEosId());
          segments.push_back(segment);

          auto astart = snt_alignment.begin() + offset;
          TokenRanges segment_alignment(astart, snt_alignment.end());
          sourceRanges.push_back(segment_alignment);
        }

      } else {
        timer::Timer push_timer;
        tokenized_sentence.push_back(tokenizer_.sourceEosId());
        segments.push_back(tokenized_sentence);
        sourceRanges.push_back(snt_alignment);
        // LOG(info, "Push took {:.5f} seconds", push_timer.elapsed());
      }
    }
    ++sentencesProcessed;
    if (sentencesProcessed % 10000 == 0) {
      LOG(info, "Processed {}", sentencesProcessed);
    }
  }
}

} // namespace bergamot
} // namespace marian
