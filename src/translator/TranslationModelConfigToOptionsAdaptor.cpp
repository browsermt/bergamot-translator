/*
 * TranslationModelConfigToOptionsAdaptor.cpp
 *
 */
#include <memory>

#include "TranslationModelConfigToOptionsAdaptor.h"

TranslationModelConfigToOptionsAdaptor::TranslationModelConfigToOptionsAdaptor() {}

TranslationModelConfigToOptionsAdaptor::~TranslationModelConfigToOptionsAdaptor() {}

std::shared_ptr<marian::Options>
TranslationModelConfigToOptionsAdaptor::adapt(const TranslationModelConfiguration& config) {
	// ToDo: Add actual implementation
	return std::make_shared<marian::Options>();
}
