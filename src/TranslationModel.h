/*
 * TranslationModel.h
 *
 * Main interface for translation API.
 */

#ifndef SRC_TRANSLATOR_TRANSLATIONMODEL_H_
#define SRC_TRANSLATOR_TRANSLATIONMODEL_H_

#include <future>
#include <string>
#include <vector>

// All 3rd party includes
#include "3rd_party/marian-dev/src/common/options.h"

// All local project includes
#include "TranslationRequest.h"
#include "TranslationResult.h"
#include "translator/definitions.h"
#include "translator/service.h"

/* A Translation model that translates a plain (without any markups and emojis)
 * UTF-8 encoded text. This implementation supports translation from 1 source
 * language to 1 target language.
 */
class TranslationModel {
public:
  /* Construct the model using the model configuration options as yaml-formatted
   * string
   */
  /**
   * @param config Marian yml config file in the form of a string
   * @param model_memory optional byte array (aligned to 64!!!) that contains
   * the bytes of a model.bin.
   */
  TranslationModel(const std::string &config,
                   marian::bergamot::AlignedMemory modelMemory = marian::bergamot::AlignedMemory(),
                   marian::bergamot::AlignedMemory shortlistMemory = marian::bergamot::AlignedMemory());

  ~TranslationModel();

  /* This method performs translation on a list of UTF-8 encoded plain text
   * (without any markups or emojis) and returns a list of results in the same
   * order. The model supports translation from 1 source language to 1 target
   * language.
   *
   * Each text entry can either be a word, a phrase, a sentence or a list of
   * sentences. Additional information related to the translated text can be
   * requested via TranslationRequest which is applied equally to each text
   * entry. The translated text corresponding to each text entry and the
   * additional information (as specified in the TranslationRequest) is
   * encapsulated and returned in TranslationResult.
   *
   * The API splits each text entry into sentences internally, which are then
   * translated independent of each other. The translated sentences are then
   * joined back together and returned in TranslationResult.
   *
   * Please refer to the TranslationRequest class to find out what additional
   * information can be requested. The alignment information can only be
   * requested if the model supports it (check isAlignmentSupported() API).
   *
   * The texts argument will become empty after the execution of this API (each
   * entry of texts list will be moved to its corresponding TranslationResult
   * object).
   */
  std::vector<TranslationResult> translate(std::vector<std::string> &&texts,
                                           TranslationRequest request);

  /* Check if the model can provide alignment information b/w original and
   * translated text. */
  bool isAlignmentSupported() const;

private:
  // Model configuration options
  std::shared_ptr<marian::Options> configOptions_; // ORDER DEPENDECNY
  marian::bergamot::Service service_;              // ORDER DEPENDENCY
};

#endif /* SRC_TRANSLATOR_TRANSLATIONMODEL_H_ */
