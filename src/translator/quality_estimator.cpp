#include <sstream>
#include "quality_estimator.h"


namespace marian {
namespace bergamot {

QualityEstimator::QualityEstimator(const char* file_parameters) {
  std::string current_str(file_parameters);
  std::string delimiter = "\n";
  size_t pos = 0;
  int line_position = 0;
  std::string line;
  while ((pos = current_str.find(delimiter)) != std::string::npos) {
      line = current_str.substr(0, pos);
      switch (line_position) {
        case 0:
            QualityEstimator::initVector(this->stds, line);
            break;
        case 1:
            QualityEstimator::initVector(this->means, line);
            break;
        case 2:
            QualityEstimator::initVector(this->coefficients, line);
            break;
        }
      current_str.erase(0, pos + delimiter.length());
      line_position++;
    }
    // last line
   QualityEstimator::initVector(this->intercept, current_str);
}

void QualityEstimator::initVector(std::vector<float> &emptyVector, std::string line){
    std::istringstream ss(line);
  
    std::string elem;
    while (ss >> elem) 
    {
        emptyVector.push_back(std::stof(elem));
    }
}

void QualityEstimator::compute_quality_scores(AnnotatedText source) {
    std::cout << source.text;
}


}
}