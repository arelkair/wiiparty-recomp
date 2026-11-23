#include "wp/build_info.h"

#include <string>

#ifndef WP_BUILD_FINGERPRINT
#define WP_BUILD_FINGERPRINT "unknown"
#endif

namespace wp::build_info {

namespace {

#if defined(__x86_64__) || defined(_M_X64)
constexpr const char* kArchitecture = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
constexpr const char* kArchitecture = "aarch64";
#else
constexpr const char* kArchitecture = "other";
#endif

#ifdef __FMA__
constexpr const char* kFma = "on";
#else
constexpr const char* kFma = "off";
#endif

#ifdef __AVX2__
constexpr const char* kAvx2 = "on";
#else
constexpr const char* kAvx2 = "off";
#endif

}

const char* fingerprint() {
    static const std::string value =
        std::string(WP_BUILD_FINGERPRINT) + " target " + kArchitecture + " compiled-with-fma " + kFma + " compiled-with-avx2 " + kAvx2 + " gcc " +
#ifdef __VERSION__
        __VERSION__;
#else
        "unknown";
#endif
    return value.c_str();
}

}
