/*
 * TranslationRequest.h
 *
 *  This file defines the translation request class to be used in
 *  TranslationModel::translate() API.
 */

#ifndef SRC_TRANSLATOR_TRANSLATIONREQUEST_H_
#define SRC_TRANSLATOR_TRANSLATIONREQUEST_H_

#include "QualityScore.h"

/* This class specifies the information related to the translated text (e.g.
 * quality of the translation etc.) that can be included in the
 * TranslationResult. These optional requests are set/unset independent of each
 * other i.e. setting any one of them doesn’t have the side effect of setting
 * any of the others.
 */
class TranslationRequest {
private:
  // The granularity for which Quality scores of the translated text will be
  // included in TranslationResult. QualityScoreGranularity::NONE means the
  // scores are not included in TranslationResult.
  QualityScoreGranularity qualityScoreGranularity =
      QualityScoreGranularity::NONE;

  // A flag to include/exclude the information regarding how individual
  // sentences of original text map to corresponding translated sentences in
  // joined translated text in the TranslationResult. An example of sentence
  // mappings:
  //     originalText (containing 2 sentences)              = "What is your
  //     name? My name is Abc." translatedText (containing 2 translated
  //     sentences) = "Was ist dein Name? Mein Name ist Abc." sentenceMappings =
  //     [
  //         {"What is your name?", "Was ist dein Name?"},  //
  //         Pair(originalText[0],translatedText[0])
  //         {"My name is Abc", "Mein Name ist Abc."}       //
  //         Pair(originalText[1],translatedText[1])
  //     ]
  bool includeSentenceMapping = false;

public:
  TranslationRequest() {}

  TranslationRequest(const TranslationRequest &request)
      : qualityScoreGranularity(request.qualityScoreGranularity),
        includeSentenceMapping(request.includeSentenceMapping) {}

  ~TranslationRequest() {}

  /* Set the granularity for which the Quality scores of translated text should
   * be included in the TranslationResult. By default
   * (QualityScoreGranularity::NONE), scores are not included.
   */
  void setQualityScoreGranularity(QualityScoreGranularity granularity) {
    qualityScoreGranularity = granularity;
  }

  /* Set to true/false to include/exclude the information regarding how
   * individual sentences of original text map to corresponding translated
   * sentences in joined translated text in the TranslationResult. By default
   * (false), this information is not included.
   */
  void sentenceMappingInResult(bool includeMapping) {
    includeSentenceMapping = includeMapping;
  }

  /* Return the granularity for which the Quality scores of the translated text
   * will be included in TranslationResult. QualityScoreGranularity::NONE means
   * the scores will not be included.
   */
  QualityScoreGranularity getQualityScoreGranularity() const {
    return qualityScoreGranularity;
  }

  /* Return whether the information regarding how individual sentences of
   * original text map to corresponding translated sentences in joined
   * translated text will be included in the TranslationResult. By default
   * (false) means this information will not be included.
   */
  bool sentenceMappingInResult() const { return includeSentenceMapping; }
};

#endif /* SRC_TRANSLATOR_TRANSLATIONREQUEST_H_ */
