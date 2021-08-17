#pragma once

#include <memory>

#include "definitions.h"
#include "iquality_model.h"

namespace marian::bergamot {

class QualityModelFactory {
 public:
  static std::shared_ptr<IQualityModel> Make(const AlignedMemory& qualityFileMemory);
};
}  // namespace marian::bergamot
