#pragma once

#include <memory>

#include "iquality_model.h"

#include "response.h"

namespace marian::bergamot {

class QualityModelFactory {
 public:
  static std::shared_ptr< IQualityModel > Make( const Ptr<Options>& options, const MemoryBundle& memoryBundle );
};
}  // namespace marian::bergamot
