#ifndef BERGAMOT_APP_CLI_H
#define BERGAMOT_APP_CLI_H
#include <algorithm>
#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/byte_array_util.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/response_options.h"
#include "translator/service.h"

namespace marian {
namespace bergamot {}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_APP_CLI_H
