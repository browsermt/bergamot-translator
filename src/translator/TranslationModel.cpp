/*
 * TranslationModel.cpp
 *
 */

#include <future>
#include <vector>

#include "TranslationModel.h"

TranslationModel::TranslationModel(const TranslationModelConfiguration &configuration) :
		modelConfiguration(configuration), AbstractTranslationModel() {
}

TranslationModel::~TranslationModel() {}

std::future<std::vector<TranslationResult>> TranslationModel::translate(
		std::vector<std::string> &&texts, TranslationRequest request) {
	//ToDo: Replace this code with the actual implementation
	return std::async([]() {
		std::vector<TranslationResult> results;
		results.emplace_back(TranslationResult{"a","d"});
		return results;
	});
}

bool TranslationModel::isAlignmentSupported() const {
	return false;
}
