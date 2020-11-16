/*
 * This class adapts the TranslationModelConfiguration object to marian::Options object.
 * marian::Options is a class that is specific to Marian and is used heavily inside it
 * as configuration options (even for translation workflow). It makes sense to work with
 * this class internally in the implementation files.
 */

#ifndef SRC_TRANSLATOR_TRANSLATIONMODELCONFIGTOOPTIONSADAPTOR_H_
#define SRC_TRANSLATOR_TRANSLATIONMODELCONFIGTOOPTIONSADAPTOR_H_

#include <memory>

// All 3rd party includes
#include "3rd_party/marian-dev/src/common/options.h"

// All local includes
#include "TranslationModelConfiguration.h"


class TranslationModelConfigToOptionsAdaptor {
public:

	TranslationModelConfigToOptionsAdaptor();

	~TranslationModelConfigToOptionsAdaptor();

	/* Create an Options object from the translation model configuration object.
	 */
	std::shared_ptr<marian::Options> adapt(const TranslationModelConfiguration& config);
};

#endif /* SRC_TRANSLATOR_TRANSLATIONMODELCONFIGTOOPTIONSADAPTOR_H_ */
