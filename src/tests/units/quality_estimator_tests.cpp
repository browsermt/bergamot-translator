
#include "catch.hpp"

#include "translator/quality_estimator.h"

using namespace marian::bergamot;

SCENARIO("Quality Estimator Test - MAP ", "[QualityEstimator]")
{
   GIVEN( "A quality, target and index" )
   {
      int sequenceIndex = 0;
      Quality quality;
      quality.sequence = 450.814026;
      quality.word = {
         24.1778,
         26.3681,
         25.5426,
         32.1016,
         21.6733,
         22.6086,
         20.9368,
         25.3221,
         32.8938,
         21.4369,
         29.6233,
         34.838,
         20.4302,
         21.2614,
         8.04761,
         17.9219,
         11.1671,
         17.1926,
         19.4645,
         17.8058 };



      // QualityEstimator qualityEstimator;



   }
}


