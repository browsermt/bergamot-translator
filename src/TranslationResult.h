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

#include "translator/sentence_ranges.h"

/// SentenceRanges is a pseudo-2D container which holds a sequence of
/// string_views that correspond to words in vocabulary. The API allows adding a
/// sentence, which accounts extra extra annotations to mark sentences in a flat
/// vector<string_views>.
typedef marian::bergamot::SentenceRangesT<std::string_view> SentenceRanges;

/// An AnnotatedBlob is blob std::string along with the annotations which are
/// valid until the original blob is valid (string shouldn't be invalidated).
/// Annotated Blob, due to its nature binding a blob to string_view should only
/// be move-constructible as a whole.
class AnnotatedBlob {
public:
  AnnotatedBlob(std::string &&blob, SentenceRanges &&ranges)
      : blob_(std::move(blob)), ranges_(std::move(ranges)) {}
  AnnotatedBlob(AnnotatedBlob &&other)
      : blob_(std::move(other.blob_)), ranges_(std::move(other.ranges_)) {}
  AnnotatedBlob(const AnnotatedBlob &other) = delete;
  AnnotatedBlob &operator=(const AnnotatedBlob &other) = delete;

  const size_t numSentences() const { return ranges_.numSentences(); }

private:
  std::string blob_;
  SentenceRanges ranges_;
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
/// further extends and stores information like alignment (which is a function)
/// of both.
///
/// The API allows access to the source and translation (text) as a whole.
/// Additional access to iterate through internal meaningful units of
/// translation (sentences) are provided through for-looping through size().
/// Might be reasonable to provide iterator access.
///
/// Wondering if we can do pImpl and forward access calls to the marian object,
/// specifying only interface here. Thus decoding and extracting alignments
/// (passable), QualityScores (expensive for sure) are done only if required.
/// This way only histories need to be held.

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

  const std::string &source() const { return source_.blob; };
  const std::string &translation() const { return translation_.blob; };

  /// Returns the number of units in a sentence.
  const size_t size() { return source_.numSentences(); }

  /// Allows access to a sentence mapping, when accesed iterating through
  /// indices using size(). Intended to substitute for getSentenceMappings.
  std::pair<const std::string_view, const std::string_view>
  sentences(size_t index) {
    return make_pair(source_.ranges.sentence(i),
                     translation_.ranges.sentence(i));
  }

  /// Both quality and alignment will require tapping into token level.
  /// information.

  /// Returns quality score associated with the i-th pair.
  Quality quality(size_t index) {
    Quality dummy;
    return dummy
  }

  /// Returns alignment information for the i-th pair.
  Alignment alignment(size_t index) {}

private:
  AnnotatedBlob source_;
  AnnotatedBlob translation_;
  std::vector<Alignment>
      alignments_; // Alignments are consistent with the word annotations for
                   // the i-th source and target sentence.
  std::vector<Quality>
      scores_; // scores_ are consistent with the word annotations for
               // the i-th source and target sentence.
};

#endif /* SRC_TRANSLATOR_TRANSLATIONRESULT_H_ */
