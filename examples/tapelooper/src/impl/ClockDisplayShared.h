#pragma once

#include <cstddef>

// Single definition of the iris's angular resolution: TapeLooperImpl (write-side
// tagging) and CircularTapeDisplay (read-side lookup) must agree exactly.
namespace TapeLooperDetail
{
constexpr size_t kIrisAngularBuckets = 2048;
}
