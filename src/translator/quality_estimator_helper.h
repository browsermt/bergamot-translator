#pragma once

#include <memory>

#include "definitions.h"
#include "iquality_estimator.h"

namespace marian::bergamot {

/// The createQualityEstimator method create a quality estimator
/// By default, if the qualityFileMemory is empty it will use
/// the unsupervised learning approach (UnsupervisedQE).
std::shared_ptr<IQualityEstimator> createQualityEstimator(const AlignedMemory& qualityFileMemory);

}  // namespace marian::bergamot
