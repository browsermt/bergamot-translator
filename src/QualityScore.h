/*
 * QualityScore.h
 *
 */

#ifndef SRC_TRANSLATOR_QUALITYSCORE_H_
#define SRC_TRANSLATOR_QUALITYSCORE_H_

#include <string>
#include <vector>

/* All possible Granularities for which Quality Scores can be returned for
 * translated text. */
enum class QualityScoreGranularity {
  WORD,
  SENTENCE,
  NONE,
};

/* This class represents the Quality Scores for various spans of a translated
 * text at a specific granularity. */
class QualityScore {
private:
  // Sections of the translated text for the Quality Scores.
  std::vector<std::string_view> textViews;

  // Quality Scores corresponding to each entry of textViews in the same order
  std::vector<float> textScores;

  // Granularity of the text for the Quality scores above
  QualityScoreGranularity textGranularity;

public:
  // ToDo: Public Methods
};

#endif /* SRC_TRANSLATOR_QUALITYSCORE_H_ */
