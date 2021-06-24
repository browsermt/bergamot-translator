#ifndef SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#define SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include "annotation.h"
#include "response.h"
#include "intgemm/intgemm.h"
#include "intgemm/aligned.h" 
#include "intgemm/callbacks.h"

namespace marian {
namespace bergamot {

class QualityEstimator {
    private:
        struct FeatureVector{
            FeatureVector(float x, float y, float w, float z): 
                feature_vector{x,y,z,w} {}
            float feature_vector[4];
        };
        std::vector<float> stds, means, coefficients, intercept;
        std::vector<std::map<std::tuple<size_t, size_t>, float>> mapping;
        void initVector(std::vector<float> &emptyVector, std::string line);
        void mapBPEToWords(Response& sentence, Words words);
        intgemm::AlignedVector<QualityEstimator::FeatureVector>  extractFeatures(Response& sentence);
        std::vector<int> predictWordLevel(std::vector<std::vector<float>> feature_vector);

    public:
        QualityEstimator(const char *file_parameters);
        void compute_quality_scores(Response& sentence, Words words);
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
