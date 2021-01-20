/*
 * main.cpp
 *
 * An example application to demonstrate the use of Bergamot translator.
 *
 */

#include <iostream>

#include "TranslationModelConfiguration.h"
#include "AbstractTranslationModel.h"
#include "TranslationRequest.h"
#include "TranslationResult.h"


int main(int argc, char** argv) {

	// Create an instance of AbstractTranslationModel with a dummy model configuration
	TranslationModelConfiguration config("dummy_modelFilePath",
				"dummy_sourceVocabPath",
				"dummy_targetVocabPath");
	std::shared_ptr<AbstractTranslationModel> model =
			AbstractTranslationModel::createInstance(config);

	// Call to translate texts with a dummy (empty) translation request
	TranslationRequest req;
	std::vector<std::string> texts = {"Hola", "Mundo"};
	std::cout << "Input size: " << texts.size() << std::endl;
	for (auto count = 0; count < texts.size(); count++) {
        std::cout << " val:" << texts[count] << std::endl;
	}

	auto result = model->translate(std::move(texts), req);

	std::cout << "Input size after translation: " << texts.size() << std::endl;
	for (auto count = 0; count < texts.size(); count++) {
        std::cout << " val:" << texts[count] << std::endl;
	}
	std::cout << "Result size: " << result.size() << std::endl;
	for (auto count = 0; count < result.size(); count++) {
        std::cout << " Original:" << result[count].getOriginalText() <<
        ", Translated:" << result[count].getTranslatedText() << std::endl;
	}
	return 0;
}
