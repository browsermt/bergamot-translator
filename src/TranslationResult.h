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

#include "translator/sentence_ranges.h"

/// Annotation is a pseudo-2D container which holds a sequence of
/// string_views that correspond to words in vocabulary from either source or
/// the translation. The API allows adding a sentence, which accounts extra
/// extra annotation to mark sentences in a flat vector<string_views> storing
/// them in an efficient way.

/// With Annotation construct in place, we bind an std::string and string_views
/// that are valid referring to the std::string in an AnnotatedBlob.
/// AnnotatedBlob doesn't have restricting it to marian::bergamot only. For DRY
/// for development and debugging purposes, it's typedef-ed using a structure
/// currently marian internal.
typedef marian::bergamot::AnnotatedBlobT<std::string_view> AnnotatedBlob;

/// Alignment is stored as a sparse matrix, this pretty much aligns with marian
/// internals but is brought here to maintain translator
/// agnosticism/independence.
struct Point {
  size_t src; // Index pointing to source string_view.
  size_t tgt; // Index pointing to target string_view.
  float prob; // Score between [0, 1] on indicating degree of alignment.
};

typedef std::vector<Point> Alignment;

struct Quality {
  float sequence; /// Certainty/uncertainty score for sequence.
  /// Certainty/uncertainty for each word in the sequence.
  std::vector<float> word;
};

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

  /// Allows access to a sentence mapping, when accesed iterating through
  /// indices using size(). Intended to substitute for getSentenceMappings.

  /// Access to source sentences.
  std::string_view source_sentence(size_t idx) const {
    return source_.annotation.sentence(idx);
  };

  /// Access to translated sentences.
  std::string_view translated_sentence(size_t idx) const {
    return translation_.annotation.sentence(idx);
  };

  /// Access to word level annotation in source.
  std::string_view source_word(size_t sentenceIdx, size_t wordIdx) const {
    return source_.annotation.word(sentenceIdx, wordIdx);
  }

  /// Access to word level annotation in translation
  std::string_view translation_word(size_t sentenceIdx, size_t wordIdx) const {
    return translation_.annotation.word(sentenceIdx, wordIdx);
  }

  /// Both quality and alignment will require tapping into token level.
  /// information. wordIdx information required to extract corresponding
  /// word-unit is embedded internally as indices in vectors (Quality) or
  /// sparse-matrix constructions with indices (Alignment).

  /// Returns quality score associated with the i-th pair.
  Quality quality(size_t index) const { return scores_[index]; }

  /// Returns alignment information for the i-th pair.
  const Alignment &alignment(size_t index) const { return alignments_[index]; }

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

  typedef std::vector<std::pair<const std::string_view, const std::string_view>>
      SentenceMappings;

  SentenceMappings getSentenceMappings() const {
    SentenceMappings mappings;
    for (size_t idx = 0; idx < size(); idx++) {
      mappings.emplace_back(source_sentence(idx), translated_sentence(idx));
    }
    return mappings;
  };

private:
  AnnotatedBlob source_;
  AnnotatedBlob translation_;
  std::vector<Alignment>
      alignments_; // Indices provided by alignments are consistent with the
                   // word annotations for the i-th source and target sentence.
  std::vector<Quality>
      scores_; // Indices provided by scores_ are consistent with the word
               // annotations annotation for the i-th target sentence.
};

#endif /* SRC_TRANSLATOR_TRANSLATIONRESULT_H_ */
