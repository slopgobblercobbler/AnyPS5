#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static_assert(sizeof(GuestItimerval) == 32 && offsetof(GuestItimerval, value) == 16);

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    for (int invalid : {-1, 3, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        GuestItimerval output{{11, 22}, {33, 44}};
        const auto saved = output;
        *__error_nid_postfix() = 0;
        Require(getitimer_nid_postfix(invalid, &output) == -1 && *__error_nid_postfix() == 22, "invalid getitimer which parameter did not report EINVAL");
        Require(std::memcmp(&output, &saved, sizeof(output)) == 0, "invalid getitimer parameter changed output");
        *__error_nid_postfix() = 0;
        Require(getitimer_nid_postfix(invalid, nullptr) == -1 && *__error_nid_postfix() == 22, "invalid getitimer parameter with null output order changed");
    }

    *__error_nid_postfix() = 0;
    Require(getitimer_nid_postfix(0, nullptr) == -1 && *__error_nid_postfix() == 14, "getitimer null output did not report EFAULT");

    for (int validTimer : {0, 1, 2}) {
        GuestItimerval output{{11, 22}, {33, 44}};
        *__error_nid_postfix() = 12345;
        Require(getitimer_nid_postfix(validTimer, &output) == 0, "valid getitimer failed");
        Require(*__error_nid_postfix() == 12345, "getitimer changed errno on success");
        Require(output.interval.tv_sec >= 0 && output.interval.tv_usec >= 0 && output.interval.tv_usec < 1000000, "invalid interval normalization");
        Require(output.value.tv_sec >= 0 && output.value.tv_usec >= 0 && output.value.tv_usec < 1000000, "invalid value normalization");
        *__error_nid_postfix() = 0;
        Require(getitimer_nid_postfix(validTimer, nullptr) == -1 && *__error_nid_postfix() == 14, "valid getitimer null output did not report EFAULT");
    }
}
