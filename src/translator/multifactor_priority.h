#ifndef SRC_BERGAMOT_MULTIFACTOR_PRIORITY_H_
#define SRC_BERGAMOT_MULTIFACTOR_PRIORITY_H_

#include "data/types.h"
#include "definitions.h"
#include "sys/time.h"

namespace marian {
namespace bergamot {

struct MultiFactorPriority {
  int nice; /* user configurable priority, at a request */
  unsigned int Id;
  /* What else should priority depend on? */
  double priority() { return Id; }
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_MULTIFACTOR_PRIORITY_H_
