/*
 * AbstractTranslationModel.cpp
 *
 */
#include <memory>

// All 3rd party includes
//#include "3rd_party/marian-dev/src/common/options.h"

// All local includes
#include "AbstractTranslationModel.h"
#include "TranslationModel.h"
//#include "TranslationModelConfigToOptionsAdaptor.h"


std::shared_ptr<AbstractTranslationModel>
AbstractTranslationModel::createInstance(const TranslationModelConfiguration& config) {
	/*TranslationModelConfigToOptionsAdaptor adaptor;
	auto options = adaptor.adapt(config);*/
	return std::make_shared<TranslationModel>(config);
}
