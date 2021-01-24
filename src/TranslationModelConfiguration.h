/*
 * TranslationModelConfiguration.h
 *
 */

#ifndef SRC_TRANSLATOR_TRANSLATIONMODELCONFIGURATION_H_
#define SRC_TRANSLATOR_TRANSLATIONMODELCONFIGURATION_H_

#include <string>

/* This class encapsulates the configuration that is required by a translation
 * model to perform translation.
 */
class TranslationModelConfiguration {
public:
  // Constructor
  TranslationModelConfiguration(const std::string &modelFilePath,
                                const std::string &sourceVocabPath,
                                const std::string &targetVocabPath)
      : modelPath(modelFilePath), sourceLanguageVocabPath(sourceVocabPath),
        targetLanguageVocabPath(targetVocabPath) {}

  // Copy constructor
  TranslationModelConfiguration(const TranslationModelConfiguration &rhs)
      : modelPath(rhs.modelPath),
        sourceLanguageVocabPath(rhs.sourceLanguageVocabPath),
        targetLanguageVocabPath(rhs.targetLanguageVocabPath) {}

  // Move constructor
  TranslationModelConfiguration(TranslationModelConfiguration &&rhs)
      : modelPath(std::move(rhs.modelPath)),
        sourceLanguageVocabPath(std::move(rhs.sourceLanguageVocabPath)),
        targetLanguageVocabPath(std::move(rhs.targetLanguageVocabPath)) {}

  // Return the path of the model file
  const std::string &getModelFilePath() const { return modelPath; }

  // Return the path of the source language vocabulary file
  const std::string &getSourceVocabularyPath() const {
    return sourceLanguageVocabPath;
  }

  // Return the path of the target language vocabulary file
  const std::string &getTargetVocabularyPath() const {
    return targetLanguageVocabPath;
  }

private:
  // Path to the translation model file
  const std::string modelPath;

  // Path to the source vocabulary file to be used by the model
  const std::string sourceLanguageVocabPath;

  // Path to the target vocabulary file to be used by the model
  const std::string targetLanguageVocabPath;

  // ToDo: Add other user configurable options (e.g. min batch size)
};

#endif /* SRC_TRANSLATOR_TRANSLATIONMODELCONFIGURATION_H_ */
