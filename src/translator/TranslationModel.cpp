/*
 * TranslationModel.cpp
 *
 */

#include <future>
#include <vector>

#include "TranslationModel.h"

TranslationModel::TranslationModel(const TranslationModelConfiguration& config) :
		configOptions(config), AbstractTranslationModel() {
}

TranslationModel::~TranslationModel() {}

std::vector<TranslationResult> TranslationModel::translate(
		std::vector<std::string> &&texts, TranslationRequest request) {
	//ToDo: Replace this code with the actual implementation
	std::vector<TranslationResult> results;
	for (auto count=0; count < texts.size(); count++) {
		results.emplace_back(std::move(texts[count]), "TRANSLATION" + std::to_string(count));
	}
	return results;
}

bool TranslationModel::isAlignmentSupported() const {
	return false;
}
