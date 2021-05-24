#ifndef SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#define SRC_BERGAMOT_QUALITY_ESTIMATOR_H_

#include <iostream>
#include <fstream>

#define ONNX_API
#include "response.h"
#include "quality_estimator.h"
#include "3rd_party/onnx/onnx/onnx/onnx.pb.h"

namespace marian {
namespace bergamot {

class QualityEstimator {
  public:
    onnx::ModelProto model;
    Response response_;
    QualityEstimator(Response &response, std::string onnx_path) {
      std::ifstream in(onnx_path, std::ios_base::binary);
      model.ParseFromIstream(&in);
      in.close();
      response_ = response;
    }

    Response updateQualityScores();
};
} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
