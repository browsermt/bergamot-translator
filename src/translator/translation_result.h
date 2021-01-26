#ifndef SRC_BERGAMOT_TRANSLATION_RESULT_H_
#define SRC_BERGAMOT_TRANSLATION_RESULT_H_

#include "data/types.h"
#include "definitions.h"
#include "translator/beam_search.h"

#include <cassert>
#include <string>
#include <vector>

namespace marian {
namespace bergamot {
class TranslationResult {
public:
  TranslationResult(std::string &&source,
                    std::vector<TokenRanges> &&sourceRanges,
                    Histories &&histories,
                    std::vector<Ptr<Vocab const>> &vocabs);

  TranslationResult(TranslationResult &&other)
      : source_(std::move(other.source_)),
        translation_(std::move(other.translation_)),
        sourceRanges_(std::move(other.sourceRanges_)),
        sentenceMappings_(std::move(other.sentenceMappings_)),
        histories_(std::move(other.histories_)){};

  TranslationResult(const TranslationResult &) = delete;
  TranslationResult &operator=(const TranslationResult &) = delete;

  // Returns const references to source and translated texts, for external
  // consumption.

  const std::string &getOriginalText() const { return source_; }
  const std::string &getTranslatedText() const { return translation_; }

  // A mapping of string_views in the source_ and translation_ are provide as a
  // pair for external consumption. Each entry corresponding
  // to a (source-sentence, target-sentence).

  typedef std::vector<std::pair<const string_view, const string_view>>
      SentenceMappings;
  const SentenceMappings &getSentenceMappings() const {
    return sentenceMappings_;
  }

  // Return the Quality scores of the translated text.
  // Not implemented currently, commenting out.
  // const QualityScore &getQualityScore() const { return qualityScore; }

  // For development use to benchmark with marian-decoder.
  const Histories &getHistories() const { return histories_; }

  // @jerinphilip: Why are these members no longer-private? For move-semantics
  // with consistent string_views for bergamot-translator.

  std::string source_;
  std::string translation_;
  // Adding the following to complete bergamot-translator spec, redundant while
  // sourceMappings_ and targetMappings_ exists or vice-versa.

  SentenceMappings sentenceMappings_;

private:
  // Histories are currently required for interoperability with OutputPrinter
  // and OutputCollector and hence comparisons with marian-decoder.
  // Future hook to gain alignments.
  Histories histories_;

  // string_views at the token level.
  std::vector<TokenRanges> sourceRanges_;
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_TRANSLATION_RESULT_H_
