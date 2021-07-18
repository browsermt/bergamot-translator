#pragma once

#include <ostream>

#include "translator/annotation.h"

bool operator==(const std::vector<marian::bergamot::ByteRange>& value1,
                const std::vector<marian::bergamot::ByteRange>& value2);
bool operator==(const std::vector<float>& value1, const std::vector<float>& value2);
std::ostream& operator<<(std::ostream& os, const marian::bergamot::ByteRange& value);