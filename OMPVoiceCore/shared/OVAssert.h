#pragma once

#include "OVLogger.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef NDEBUG
#define OV_ASSERT(expr) do { if (!(expr)) { OV_LOG_ERROR("Assert", "ASSERT FAILED: %s", #expr); } } while (false)
#else
#ifdef _MSC_VER
#define OV_ASSERT(expr) do { if (!(expr)) { OV_LOG_ERROR("Assert", "ASSERT FAILED: %s", #expr); __debugbreak(); } } while (false)
#else
#define OV_ASSERT(expr) do { if (!(expr)) { OV_LOG_ERROR("Assert", "ASSERT FAILED: %s", #expr); __builtin_trap(); } } while (false)
#endif
#endif
