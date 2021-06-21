#include "catch.hpp"
#include "translator/tag_processor.h"

using namespace marian::bergamot;

void testCase(const marian::data::SoftAlignment& softAlign, size_t srcLength, size_t tgtLength,
              const std::vector<TagNode>& input, const std::vector<TagNode>& expected, bool isDebug=false){
  TagProcessor tp = TagProcessor(softAlign, input, srcLength, tgtLength);
  ByteRange rootRange;
  int exitStatus = tp.traverseAndQuery(0, {0, tgtLength}, rootRange);
  std::vector<TagNode> tagTreeTarget = tp.tagTreeTarget;

  if (exitStatus == 0)
    std::cout <<"Solution found" <<std::endl;
  else
    std::cout <<"No solution" <<std::endl;

  REQUIRE(tagTreeTarget.size() == expected.size());
  for (size_t i = 0; i < tagTreeTarget.size(); i++) {
    if(isDebug)
      std::cout << i <<" "<<tagTreeTarget[i].bound.begin <<" "<<tagTreeTarget[i].bound.end << std::endl;
    if(!isDebug) {
      REQUIRE(tagTreeTarget[i].bound.begin == expected[i].bound.begin);
      REQUIRE(tagTreeTarget[i].bound.end == expected[i].bound.end);
    }
  }
}

TEST_CASE("Test Tag Nesting Features with one sentence data") {
  //[original]: A republican strategy to counteract the re-election of Obama.
  // [translated]: Eine republikanische Strategie, um der Wiederwahl Obamas entgegenzuwirken.
  // A:0  republic:1 an:2  strategy:3  to:4  counter:5 act:6  the:7  re:8 -:9 election:10  of:11  Obama:12 .:13 :14
  // Eine:0  :1 republik:2 anische:3  Strategie:4 ,:5  um:6  der:7  Wieder:8 wahl:9  Obama:10 s:11  entgegen:12 zu:13 wirken:14 .:15 :16
  marian::data::SoftAlignment softAlign =
          {{0.988377,    0.0019854,   0.00502577,  0.000275177, 0.000225201, 0.00243003,  0.00129913,  0.000130121, 4.70618e-05, 9.28533e-06, 9.31939e-06, 2.97117e-06, 1.74904e-05, 9.99673e-09, 0.000166429},
           {0.00603287,  0.907779,    0.0586549,   0.0181961,   0.00461115,  0.00160618,  0.00109232,  0.000172891, 0.00037208,  0.000191994, 4.17242e-05, 1.03908e-06, 2.842e-06,   5.83511e-06, 0.00123912},
           {9.52466e-06, 0.889603,    0.108871,    0.000119867, 4.45812e-06, 0.000565912, 0.000394552, 6.92264e-06, 6.19438e-05, 1.26698e-05, 3.10971e-05, 1.02975e-08, 3.79738e-07, 1.62942e-07, 0.000318893},
           {0.000325015, 0.198765,    0.799421,    0.000261236, 6.54766e-05, 0.000248284, 0.000224711, 1.65749e-05, 1.66763e-06, 6.27637e-05, 1.5373e-06,  4.8845e-07,  3.40976e-07, 2.36188e-06, 0.000604362},
           {0.000588019, 0.000831455, 0.00283437,  0.991928,    0.00136463,  0.000893542, 0.000855011, 2.01782e-05, 4.00796e-05, 2.09971e-05, 0.000248072, 1.30982e-06, 2.89528e-07, 7.83275e-06, 0.000366291},
           {0.00419607,  0.00442268,  0.0210147,   0.030931,    0.879956,    0.0154661,   0.0125432,   0.0158385,   0.000119378, 0.000897955, 7.52258e-05, 0.000560652, 0.00048246,  0.00244568,  0.0110505},
           {0.00167524,  0.000423503, 0.00271505,  0.00592864,  0.871172,    0.0236157,   0.00952163,  0.0807483,   0.000324147, 0.000165053, 0.000256071, 0.00019553,  0.000231757, 5.73034e-05, 0.00297003},
           {0.00046627,  0.000299897, 0.000465712, 0.000479372, 0.0201142,   0.0242985,   0.0180445,   0.920111,    0.00310686,  0.00390014,  0.00185464,  0.000572725, 0.00278143,  3.82503e-05, 0.00346688},
           {2.20529e-05, 0.000355954, 1.94187e-05, 6.72052e-05, 9.96071e-05, 0.00326188,  0.00173714,  0.00179608,  0.834708,    0.00714464,  0.149616,    9.37391e-05, 0.000888446, 1.19078e-06, 0.000188524},
           {3.20587e-06, 1.23475e-05, 1.842e-05,   2.68668e-06, 1.04936e-06, 0.000657801, 0.000481416, 0.000349324, 0.0375079,   0.0394762,   0.920877,    4.40539e-05, 0.000183422, 6.42078e-08, 0.000385132},
           {7.07719e-05, 1.05197e-05, 4.31596e-05, 1.66167e-05, 0.000233253, 0.000521305, 0.000541678, 0.018079,    0.000445836, 0.0145171,   0.00350198,  0.387499,    0.573413,    3.75867e-06, 0.00110345},
           {0.0283261,   0.00565204,  0.0152517,   0.00378567,  0.0206986,   0.220013,    0.0942703,   0.137227,    0.027469,    0.0617384,   0.0216684,   0.248306,    0.0758068,   0.000620263, 0.0391676},
           {0.0210442,   0.00150153,  0.00181773,  0.0134545,   0.0896248,   0.583359,    0.154537,    0.0372656,   0.0233456,   0.00703389,  0.0160486,   0.00890714,  0.0265896,   0.00332905,  0.0121422},
           {0.00216062,  0.00261816,  0.00508271,  0.00325453,  0.00975787,  0.640673,    0.30854,     0.00454329,  0.00415011,  0.0011781,   0.00260233,  0.00147239,  0.0025565,   0.00346376,  0.00794635},
           {0.00161244,  0.00170835,  0.00222657,  0.00731745,  0.00303413,  0.540801,    0.410867,    0.00538166,  0.00277104,  0.00264242,  0.00181986,  0.00194841,  0.00114676,  0.00598293,  0.0107395},
           {0.00124959,  0.00193582,  0.00367289,  0.00380727,  0.00429643,  0.00273162,  0.00171545,  0.000468546, 0.000129689, 0.000531916, 0.000180288, 0.000825546, 0.00170329,  0.97456,     0.00219204},
           {0.0650126,   0.0572322,   0.050356,    0.0802739,   0.101601,    0.0674414,   0.0737998,   0.0730955,   0.0628727,   0.0512813,   0.0470589,   0.0747435,   0.0646115,   0.0669867,   0.0636325}};

  size_t tgtLength = softAlign.size();
  size_t srcLength = softAlign[0].size();

  SECTION("Test the sum of P(s|t) for a given t is 1.0") {
    std::vector<TagNode> tagTree{
            TagNode({0, 14}, {})
    };

    float checkSum = 0;
    for (int t = 0; t < tgtLength; t++) {
      for (int s = 0; s < srcLength; s++) {
        checkSum += softAlign[t][s];
      }
      REQUIRE(checkSum == Approx(1.0));
      checkSum = 0;
    }
  }

  SECTION("The tag is wrapped in the whole sentence") {
    // [original]: <span>A republican strategy to counteract the re-election of Obama.</span>
    // [translated]:<span>Eine republikanische Strategie, um der Wiederwahl Obamas entgegenzuwirken.</span>
    std::vector<TagNode> tagTree{
            TagNode({0, 14}, {})
    };

    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({0, 17}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);

  }

  SECTION("The tag is wrapped in the middle of the sentence") {
    // [original]: <span>A republican strategy</span> to counteract the re-election of Obama.
    // [translated]:<span>Eine republikanische Strategie</span>, um der Wiederwahl Obamas entgegenzuwirken.
    std::vector<TagNode> tagTree{
            TagNode({0, 4}, {})
    };
    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({0, 5}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
  }

  SECTION("The tag is wrapped in the middle of the sentence case 2") {
    // [original]: A <span>republican strategy</span> to counteract the re-election of Obama.
    // [translated]:Eine <span>republikanische Strategie</span>, um der Wiederwahl Obamas entgegenzuwirken.
    std::vector<TagNode> tagTree{
            TagNode({1, 4}, {})
    };

    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({1, 5}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
  }

  SECTION("The tag is wrapped in one word [one-to-one]") {
    // [original]: <span>A</span> republican strategy to counteract the re-election of Obama.
    // [translated]:<span>Eine</span> republikanische Strategie, um der Wiederwahl Obamas entgegenzuwirken.
    std::vector<TagNode> tagTree{
            TagNode({0, 1}, {})
    };
    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({0, 1}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
  }

//  SECTION("The tag is wrapped in one word [one-to-zero]") {
//    // [original]: A republican strategy to counteract the re<span>-</span>election of Obama.
//    // [translated]: Eine republikanische Strategie, um der Wiederwahl Obamas entgegenzuwirken.
//    std::vector<TagNode> tagTree{
//            TagNode({9, 10}, {})
//    };
//    // should be empty
//    std::vector<TagNode> tagTreeTargetExpected{
//            TagNode({0, 0}, {})
//    };
//
//    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
//  }

  SECTION("Nested tags with one child") {
    // [original]: <span><span>A republican strategy</span> to counteract the re-election of Obama.</span>
    // [translated]:<span><span>Eine republikanische Strategie</span>, um der Wiederwahl Obamas entgegenzuwirken.</span>
    std::vector<TagNode> tagTree{
            TagNode({0, 14}, {1}),
            TagNode({0, 4}, {})
    };

    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({0, 17}, {1}),
            TagNode({0, 5}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
  }

  SECTION("Nested tags with two children") {
    // [original]: <span><span>A</span> republican strategy to counteract the <span>re-election</span> of Obama.</span>
    // [translated]:<span><span>Eine</span> republikanische Strategie, um der <span>Wiederwahl</span> Obamas entgegenzuwirken.</span>
    std::vector<TagNode> tagTree{
            TagNode({0, 14}, {1,2}),
            TagNode({0, 1}, {}),
            TagNode({8, 11}, {})
    };

    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({0, 17}, {1}),
            TagNode({0, 1}, {}),
            TagNode({8, 10}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
  }

  SECTION("Nested tags with two children 2") {
    // [original]: A republican strategy to <span><span>counteract</span> the re-election <span>of</span></span> Obama.
    // [translated]: Eine republikanische Strategie, um <span>der Wiederwahl <span>Obama</span>s <span>entgegenzuwirken</span></span>.
    std::vector<TagNode> tagTree{
            TagNode({5, 12}, {1,2}),
            TagNode({11, 12}, {}),
            TagNode({5, 7}, {})
    };
    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({7, 15}, {1,2}),
            TagNode({10, 11}, {}),
            TagNode({12, 15}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
  }

  SECTION("Two-layer nested tags with one child") {
    // [original]: <span><span><span>A republican strategy</span> to counteract the re-election of Obama.</span>
    // [translated]:<span><span><span>Eine</span> republikanische Strategie</span>, um der Wiederwahl Obamas entgegenzuwirken.</span>
    std::vector<TagNode> tagTree{
            TagNode({0, 14}, {1}),
            TagNode({0, 4}, {2}),
            TagNode({0, 1}, {})
    };

    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({0, 17}, {1}),
            TagNode({0, 5}, {2}),
            TagNode({0, 1}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
  }

  SECTION("Two-layer nested tags with two children 2") {
    // [original]: A republican strategy to <span><span>counteract <span>the</span></span> re-election <span>of</span></span> Obama.
    // [translated]: Eine republikanische Strategie, um <span>der Wiederwahl <span>Obama</span><span><span>s</span> entgegenzuwirken</span></span>.
    std::vector<TagNode> tagTree{
            TagNode({5, 12}, {1,2}),
            TagNode({11, 12}, {}),
            TagNode({5, 8}, {3}),
            TagNode({7, 8}, {}),
    };
    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({7, 15}, {1,2}),
            TagNode({10, 11}, {}),
            TagNode({11, 15}, {3}),
            TagNode({11, 12}, {}),
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected);
  }
  SECTION("Nested tag with many children [no solution case]") {
    // [original]: A republican strategy <span><span>to</span> <span>counter</span><span>act</span> <span>the</span></span> re-election of Obama.
    // [translated]: Eine republikanische Strategie, um der Wiederwahl Obamas entgegenzuwirken.
    std::vector<TagNode> tagTree{
            TagNode({4, 8}, {1,2,3,4}),
            TagNode({4, 5}, {}),
            TagNode({5, 6}, {}),
            TagNode({6, 7}, {}),
            TagNode({7, 8}, {})
    };
    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({4, 8}, {1,2,3,4}),
            TagNode({4, 5}, {}),
            TagNode({5, 6}, {}),
            TagNode({6, 7}, {}),
            TagNode({7, 8}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected, true);
  }


  SECTION("Empty tag") {
    // [original]: A republican strategy<br> to counteract the re-election of Obama.
    // [translated]: Eine republikanische Strategie, um der Wiederwahl Obamas entgegenzuwirken.
    std::vector<TagNode> tagTree{
            TagNode({2, 2}, {})
    };
    std::vector<TagNode> tagTreeTargetExpected{
            TagNode({5, 5}, {})
    };

    testCase(softAlign, srcLength, tgtLength, tagTree, tagTreeTargetExpected, true);
  }

// Cannot deal properly
//  SECTION("Two independent tags") {
//    // [original]: <span>A republican strategy</span> to counteract the <span>re-election of Obama</span>.
//    // [translated]:<span>Eine republikanische Strategie</span>, um der <span>Wiederwahl Obamas</span> entgegenzuwirken.
//    std::vector<TagNode> tagTree{
//            TagNode({0, 4}, {}),
//            TagNode({8, 13}, {})
//    };
//
//    TagProcessor tp = TagProcessor(softAlign, tagTree, srcLength, tgtLength);
//
//    tp.traverseAndQuery(0, {0, tgtLength});
//
//    std::vector<TagNode> tagTreeTarget = tp.tagTreeTarget;
//
//    for (size_t i = 0; i < tagTreeTarget.size(); i++) {
//      std::cout << i << tagTreeTarget[i].bound.begin << tagTreeTarget[i].bound.end << std::endl;
//    }
//  }
}
