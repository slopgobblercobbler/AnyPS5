#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

constexpr int GuestEbadf = 9;
constexpr int GetLock = 11;
constexpr int SetLock = 12;
constexpr std::int16_t ReadLock = 1;
constexpr std::int16_t Unlock = 2;
constexpr std::int16_t WriteLock = 3;
constexpr std::int16_t FromStart = 0;

struct GuestLock {
    std::int64_t start;
    std::int64_t length;
    std::int32_t pid;
    std::int16_t type;
    std::int16_t whence;
    std::int32_t system;
};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL FLOCK ADV: %s\n", message);
        std::abort();
    }
}

} // namespace

int main() {
    GuestLock request{};
    request.type = ReadLock;
    request.whence = FromStart;
    request.start = 0;
    request.length = 10;

    Require(fcntl_nid_postfix(-1, SetLock, &request) == -1 && *__error_nid_postfix() == GuestEbadf, "fcntl(F_SETLK) on invalid fd did not fail with EBADF");
    Require(fcntl_nid_postfix(1000000, GetLock, &request) == -1 && *__error_nid_postfix() == GuestEbadf, "fcntl(F_GETLK) on invalid fd did not fail with EBADF");

    const std::string path = (std::filesystem::temp_directory_path() / ("anyps5-flock-adv-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin")).string();
    const int fd = open_nid_postfix(path.c_str(), 0x602, 0644);
    Require(fd >= 0, "failed to create test file");

    request.type = WriteLock;
    Require(fcntl_nid_postfix(fd, SetLock, &request) == 0, "failed to set F_WRLCK");

    GuestLock query = request;
    Require(fcntl_nid_postfix(fd, GetLock, &query) == 0, "failed to get F_GETLK");
    Require(query.type == Unlock, "F_GETLK on self-owned exclusive lock did not report Unlock");

    request.type = Unlock;
    Require(fcntl_nid_postfix(fd, SetLock, &request) == 0, "failed to unlock F_WRLCK");

    Require(close_nid_postfix(fd) == 0, "failed to close fd");
    Require(std::filesystem::remove(path), "failed to remove temp file");

    std::puts("guest_kernel_flock_adv tests passed");
    return 0;
}
