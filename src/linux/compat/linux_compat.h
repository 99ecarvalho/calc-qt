// Force-included when building CalcManager on Linux.
// MSVC pulls these in transitively; libstdc++ and libc++ do not.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <algorithm>
