#pragma once
#include <ostream>
#include "translator/definitions.h"

std::ostream &operator<<(std::ostream &out, marian::bergamot::ByteRange const &b);

std::ostream &operator<<(std::ostream &out, std::pair<marian::bergamot::ByteRange,marian::bergamot::ByteRange> const &b);
