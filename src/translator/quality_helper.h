#pragma once

#include "definitions.h"

namespace marian {
namespace bergamot {

class QualityHelper
{
public:
    static AlignedMemory writeQualityEstimatorMemory( const std::vector< std::vector< float > >& logisticRegressorParameters );
};
}
}