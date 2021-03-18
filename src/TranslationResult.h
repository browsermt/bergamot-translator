/*
 * TranslationResult.h
 *
 * The class that represents the result of AbstractTranslationModel::translate()
 * API for each of its text entry and TranslationRequest.
 */

#ifndef SRC_TRANSLATOR_TRANSLATIONRESULT_H_
#define SRC_TRANSLATOR_TRANSLATIONRESULT_H_

#include <cassert>
#include <string>
#include <utility>
#include <vector>

#include "QualityScore.h"

/* This class represents the result of AbstractTranslationModel::translate() API
 * for each of its text entry and TranslationRequest.
 */

#include "translator/response.h"
#include "translator/sentence_ranges.h"

// The following are structures within Response, with no heavy marian
// dependencies, hence ripe for exporting through WASM.

typedef marian::bergamot::AnnotatedBlob AnnotatedBlob;
typedef marian::bergamot::Quality Quality;
typedef marian::bergamot::Alignment Alignment;

/// TranslationResult holds two annotated blobs of source and translation. It
/// further extends and stores information like alignment (which is a function
/// of both source and target).
///
/// The API allows access to the source and translation (text) as a whole.
/// Additional access to iterate through internal meaningful units of
/// translation (sentences) are provided through for-looping through size().
/// Might be reasonable to provide iterator access.
///
/// It is upto the browser / further algorithms to decide what to do with the
/// additional information provided through accessors. vectors alignments_ and
/// score_ are (tentatively) set only if they're demanded by TranslationRequest,
/// given they're expensive.

class TranslationResult {
public:
  // We can allow empty construction.
  TranslationResult() {}
  TranslationResult(AnnotatedBlob &&source, AnnotatedBlob &&translation)
      : source_(std::move(source)), translation_(std::move(translation)) {
    assert(source_.numSentences() == translation_.numSentences());
  }

  void set_scores(std::vector<Quality> &&quality) {
    scores_ = std::move(quality);
  }

  void set_alignments(std::vector<Alignment> &&alignments) {
    alignments_ = std::move(alignments);
  }

  // Access to source as a whole.
  const std::string &source() const { return source_.blob; };

  // Access to translation as a whole.
  const std::string &translation() const { return translation_.blob; };

  /// Returns the number of units that are translated. To simplify thinking can
  /// be thought of as number of sentences (equal in source and target by nature
  /// of the task).
  const size_t size() const { return source_.numSentences(); }

  const size_t numSourceWords(size_t sentenceIdx) {
    return source_.annotation.numWords(sentenceIdx);
  }

  const size_t numTranslationWords(size_t sentenceIdx) {
    return translation_.annotation.numWords(sentenceIdx);
  }

  /// Allows access to a sentence mapping, when accesed iterating through
  /// indices using size(). Intended to substitute for getSentenceMappings.

  /// Access to source sentences.
  marian::string_view source_sentence(size_t idx) const {
    return source_.sentence(idx);
  };

  /// Access to translated sentences.
  marian::string_view translated_sentence(size_t idx) const {
    return translation_.sentence(idx);
  };

  /// Access to word level annotation in source.
  marian::string_view source_word(size_t sentenceIdx, size_t wordIdx) const {
    return source_.word(sentenceIdx, wordIdx);
  }

  /// Access to word level annotation in translation
  marian::string_view translation_word(size_t sentenceIdx,
                                       size_t wordIdx) const {
    return translation_.word(sentenceIdx, wordIdx);
  }

  /// Both quality and alignment will require tapping into token level.
  /// information. wordIdx information required to extract corresponding
  /// word-unit is embedded internally as indices in vectors (Quality) or
  /// sparse-matrix constructions with indices (Alignment).

  /// Returns quality score associated with the i-th pair.
  const Quality &quality(size_t sentenceIdx) const {
    return scores_[sentenceIdx];
  }

  /// Returns alignment information for the i-th pair.
  const Alignment &alignment(size_t sentenceIdx) const {
    return alignments_[sentenceIdx];
  }

  const std::string_view getOriginalText() const {
    std::cerr << "DeprecationWarning: getOriginalText() is deprecated in "
                 "favour of source() to "
                 "maintain consistency."
              << std::endl;
    std::string_view sourceView(source_.blob);
    return sourceView;
  }

  const std::string_view getTranslatedText() const {
    std::cerr << "DeprecationWarning: getTranslatedText() is deprecated in "
                 "favour of translation() to "
                 "maintain consistency."
              << std::endl;
    std::string_view translatedView(translation_.blob);
    return translatedView;
  }

  typedef std::vector<std::pair<const std::string, const std::string>>
      SentenceMappings;

  SentenceMappings getSentenceMappings() const {
    SentenceMappings mappings;
    for (size_t idx = 0; idx < size(); idx++) {
      mappings.emplace_back(std::string(source_sentence(idx)),
                            std::string(translated_sentence(idx)));
    }
    return mappings;
  };

private:
  AnnotatedBlob source_;
  AnnotatedBlob translation_;
  std::vector<Alignment>
      alignments_; // Indices provided by alignments are consistent with the
                   // word annotations for the i-th source and target
                   // sentence.
  std::vector<Quality>
      scores_; // Indices provided by scores_ are consistent with the word
               // annotations annotation for the i-th target sentence.
};

#endif /* SRC_TRANSLATOR_TRANSLATIONRESULT_H_ */
