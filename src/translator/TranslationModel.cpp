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
	results.emplace_back("original1","translated1");
	results.emplace_back("original2","translated2");
	return results;
}

bool TranslationModel::isAlignmentSupported() const {
	return false;
}
