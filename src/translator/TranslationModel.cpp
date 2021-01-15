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

std::future<std::vector<TranslationResult>> TranslationModel::translate(
		std::vector<std::string> &&texts, TranslationRequest request) {
	//ToDo: Replace this code with the actual implementation
	return std::async([]() {
		std::vector<TranslationResult> results;
		return results;
	});
}

bool TranslationModel::isAlignmentSupported() const {
	return false;
}
