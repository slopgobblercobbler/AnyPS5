#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cerrno>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "sendmsg null hdr check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int EFAULT = 14;
constexpr int EBADF = 9;

static bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

int main() {
    const int sock = socket_nid_postfix(Inet, Stream, 0);
    Require(sock >= 0);

    Require(Fails(sendmsg_nid_postfix(sock, nullptr, 0), EFAULT));
    Require(Fails(sendmsg_nid_postfix(-1, reinterpret_cast<const void*>(0x1000), 0), EBADF));

    Require(close_nid_postfix(sock) == 0);
    return 0;
}
