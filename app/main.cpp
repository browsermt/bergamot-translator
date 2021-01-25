/*
 * main.cpp
 *
 * An example application to demonstrate the use of Bergamot translator.
 *
 */

#include <iostream>

#include "AbstractTranslationModel.h"
#include "TranslationRequest.h"
#include "TranslationResult.h"


int main(int argc, char** argv) {
	// Create AbstractTranslationModel instance with a configuration (as yaml-formatted string)
	std::string config("{workspace: 1024, cpu-threads: 2}");
	std::shared_ptr<AbstractTranslationModel> model =
			AbstractTranslationModel::createInstance(config);

	// Call to translate a dummy (empty) texts with a dummy (empty) translation request
	TranslationRequest req;
	std::vector<std::string> texts;
	auto result = model->translate(std::move(texts), req);

	// Resolve the future and get the actual result
	std::vector<TranslationResult> res = result.get();

	std::cout << "Count is: " << res.size() << std::endl;
	return 0;
}
