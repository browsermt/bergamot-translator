/*
 * TranslationResult.h
 *
 * The class that represents the result of AbstractTranslationModel::translate()
 * API for each of its text entry and TranslationRequest.
 */

#ifndef SRC_TRANSLATOR_TRANSLATIONRESULT_H_
#define SRC_TRANSLATOR_TRANSLATIONRESULT_H_

#include <cassert>
#include <pair>
#include <string>
#include <vector>

#include "QualityScore.h"

/* This class represents the result of AbstractTranslationModel::translate() API
 * for each of its text entry and TranslationRequest.
 */

#include "translator/sentence_annotation.h"

/// Annotation is a pseudo-2D container which holds a sequence of
/// string_views that correspond to words in vocabulary from either source or
/// the translation. The API allows adding a sentence, which accounts extra
/// extra annotation to mark sentences in a flat vector<string_views> storing
/// them in an efficient way.
typedef marian::bergamot::SentenceRangesT<std::string_view> Annotation;

/// An AnnotatedBlob is blob std::string along with the annotation which are
/// valid until the original blob is valid (string shouldn't be invalidated).
/// Annotated Blob, due to its nature binding a blob to string_view should only
/// be move-constructible as a whole.
struct AnnotatedBlob {
  std::string blob;
  Annotation annotation;

  AnnotatedBlob(std::string &&blob, Annotation &&annotation)
      : blob(std::move(blob)), annotation(std::move(annotation)) {}
  AnnotatedBlob(AnnotatedBlob &&other)
      : blob(std::move(other.blob)), annotation(std::move(other.annotation)) {}
  AnnotatedBlob(const AnnotatedBlob &other) = delete;
  AnnotatedBlob &operator=(const AnnotatedBlob &other) = delete;
  const size_t numSentences() const { return annotation.numSentences(); }
};

/// Alignment is stored as a sparse matrix, this pretty much aligns with marian
/// internals but is brought here to maintain translator
/// agnosticism/independence.
class Alignment {
public:
  struct Point {
    size_t src;  // Index pointing to source string_view.
    size_t tgt;  // Index pointing to target string_view.
    float value; // Score between [0, 1] on indicating degree of alignment.
  };

  // Additional methods can be brought about to export this as a matrix, replace
  // tags or whatever, leaving space open below.
  //
  // matrix hard_alignment()
  // vector soft_alignment()
  // Embedding tags can be written as another function
  // f(Alignment a, AnnotatedBlob source, AnnotatedBlob target)

private:
  std::vector<Point> alignment_;
};

struct Quality {
  float sequence;     // Certainty/uncertainty score for sequence.
  vector<float> word; // Certainty/uncertainty for each word in the sequence.
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
/// score_ are set only if they're demanded by TranslationRequest, given they're
/// expensive.

class TranslationResult {
public:
  TranslationResult(AnnotatedBlob &&source, AnnotatedBlob &&translation)
      : source_(std::move(source)), translation_(std::move(translation_)) {
    assert(source_.numSentences() == translation_.numSentences());
  }

  void set_scores(std::vector<Quality> &&quality) {
    scores_ = std::move(quality);
  }

  void set_alignments(std::vector<Alignment> &&alignments) {
    alignments_ = std::move(alignments);
  }

  // Access to source and translation as a whole.
  const std::string &source() const { return source_.blob; };
  const std::string &translation() const { return translation_.blob; };

  /// Returns the number of units that are translated. To simplify thinking can
  /// be thought of as number of sentences (equal in source and target by nature
  /// of the task).
  const size_t size() const { return source_.numSentences(); }

  /// Allows access to a sentence mapping, when accesed iterating through
  /// indices using size(). Intended to substitute for getSentenceMappings.

  /// Access to sentences.
  std::string_view source_sentence(size_t idx) const;
  std::string_view translated_sentence(size_t idx) const;

  /// Access to token level annotation.
  void load_source_sentence_tokens(
      size_t idx, std::vector<std::string_view> &sourceTokens) const;
  void load_target_sentence_tokens(
      size_t idx, std::vector<std::string_view> &targetTokens) const;

  /// Both quality and alignment will require tapping into token level.
  /// information.

  /// Returns quality score associated with the i-th pair.
  Quality quality(size_t index) const { return scores_[index]; }

  /// Returns alignment information for the i-th pair.
  Alignment alignment(size_t index) const { return alignments_[index]; }

private:
  AnnotatedBlob source_;
  AnnotatedBlob translation_;
  std::vector<Alignment>
      alignments_; // Alignments are consistent with the word annotation for
                   // the i-th source and target sentence.
  std::vector<Quality> scores_; // scores_ are consistent with the word
                                // annotation for the i-th target sentence.
};

#endif /* SRC_TRANSLATOR_TRANSLATIONRESULT_H_ */
