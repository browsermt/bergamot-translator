#pragma once

#include <memory>

#include "definitions.h"
#include "iquality_model.h"

namespace marian::bergamot {

/// The QualityModelFactory initialize the qualtiy model
/// By default, if the qualityFileMemory is empty it will use
/// the unsupervised learning approach (SimpleQualityModel).
class QualityModelFactory {
 public:
  static std::shared_ptr<IQualityModel> Make(const AlignedMemory& qualityFileMemory);
};
}  // namespace marian::bergamot
