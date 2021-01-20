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

	// Call to translate a dummy (empty) texts with a dummy (empty) translation request
	TranslationRequest req;
	std::vector<std::string> texts;
	auto result = model->translate(std::move(texts), req);

	std::cout << "Count is: " << result.size() << std::endl;
	return 0;
}
