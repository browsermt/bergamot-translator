#ifndef SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#define SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#include <iostream>
#include <vector>
#include <string>
#include "annotation.h"
#include "response.h"

namespace marian {
namespace bergamot {

class QualityEstimator {
    private:
        std::vector<float> stds, means, coefficients, intercept;
        void initVector(std::vector<float> &emptyVector, std::string line);

    public:
        QualityEstimator(const char *file_parameters);
        void compute_quality_scores(AnnotatedText source);
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
