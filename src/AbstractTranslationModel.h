/*
 * AbstractTranslationModel.h
 *
 * An interface for a translation model for translating a plain (without any
 * markups and emojis) UTF-8 encoded text. The model supports translation from 1
 * source language to 1 target language. There can be different implementations
 * of this interface.
 */

#ifndef SRC_TRANSLATOR_ABSTRACTTRANSLATIONMODEL_H_
#define SRC_TRANSLATOR_ABSTRACTTRANSLATIONMODEL_H_

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "TranslationRequest.h"
#include "TranslationResult.h"

/* An interface for a translation model for translating a plain (without any
 * markups and emojis) UTF-8 encoded text. The model supports translation from 1
 * source language to 1 target language.
 */
class AbstractTranslationModel {
public:
  /* A Factory method to create and return an instance of an implementation of
   * AbstractTranslationModel. The instance is created using translation model
   * configuration provided as yaml-formatted string.
   */
  static std::shared_ptr<AbstractTranslationModel>
  createInstance(const std::string &config);

  AbstractTranslationModel() = default;

  virtual ~AbstractTranslationModel() = default;

  /* This method performs translation on a list of (UTF-8 encoded) texts and
   * returns a list of results in the same order. Each text entry can either be
   * a word, a phrase, a sentence or a list of sentences and should contain
   * plain text (without any markups or emojis). Additional information related
   * to the translated text can be requested via TranslationRequest which is
   * applied equally to each text entry.
   *
   * The translated text corresponding to each text entry and the additional
   * information (as specified in the TranslationRequest) is encapsulated and
   * returned in TranslationResult.
   *
   * The API splits each text entry into sentences internally, which are then
   * translated independent of each other. The translated sentences are then
   * joined together and returned in TranslationResult. Please refer to the
   * TranslationRequest class to find out what additional information can be
   * requested. The alignment information can only be requested if the model
   * supports it (check isAlignmentSupported() API).
   *
   * The texts argument will become empty after the execution of this API (each
   * entry of texts list will be moved to its corresponding TranslationResult
   * object).
   */
  virtual std::vector<TranslationResult>
  translate(std::vector<std::string> &&texts, TranslationRequest request) = 0;

  /* Check if the model can provide alignment information b/w original and
   * translated text. */
  virtual bool isAlignmentSupported() const = 0;
};

#endif /* SRC_TRANSLATOR_ABSTRACTTRANSLATIONMODEL_H_ */
