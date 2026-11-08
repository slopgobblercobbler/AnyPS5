#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "guest savedata mountpoint check failed: %s\n", what);
    ++failures;
}

} // namespace

int main() {
    SaveDataMountPoint point{};
    SaveDataMountInfo info{};
    Check(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter, "null mount point returns parameter error");
    SaveDataMountPoint unterminated{};
    std::memset(unterminated.data, 'a', sizeof(unterminated.data));
    Check(sceSaveDataGetMountInfo(&unterminated, &info) == SaveDataErrorParameter, "unterminated mount point returns parameter error");
    return failures == 0 ? 0 : 1;
}
