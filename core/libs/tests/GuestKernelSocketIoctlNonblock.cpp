#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#include <csignal>
#endif

namespace {

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int UnixDomain = 1;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInappropriateIoctl = 25;
constexpr int ErrorAgain = 35;
constexpr int ErrorAddressNotAvailable = 49;

constexpr std::uint64_t NonblockingRequest = 0x8004667e;
constexpr std::uint64_t ReadableRequest = 0x4004667f;
constexpr std::uint64_t WindowSizeRequest = 0x40087468;
constexpr std::uint64_t EmptyInputRequest = 0x8000667f;
constexpr std::uint64_t IntegerArgumentRequest = 0x20046601;
constexpr std::uint64_t UnsupportedNetGroupRequest = 0x80046901;

constexpr std::uint64_t MalformedRequests[] = {
    0, 123, 0x5401, 0x5413, 0x40007468, 0x20087468, 0x00087468
};

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

void AuditDescriptorBounds() {
    int enable = 1;
    int queued = -1;
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(-1, NonblockingRequest, &enable) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on -1 must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(-1, NonblockingRequest, &enable) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONBIO on -1 must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(-1, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on -1 must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(-1, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONREAD on -1 must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(65535, NonblockingRequest, &enable) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on invalid descriptor must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(65535, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on invalid descriptor must fail with EBADF");

    const int closed = socket_nid_postfix(Inet, Stream, 0);
    Require(closed >= 0, "socket creation failed");
    Require(close_nid_postfix(closed) == 0, "close failed");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(closed, NonblockingRequest, &enable) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on closed socket must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(closed, NonblockingRequest, &enable) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONBIO on closed socket must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(closed, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on closed socket must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(closed, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONREAD on closed socket must fail with EBADF");
}

void AuditInvalidPointers(int socket) {
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socket, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONBIO with null pointer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socket, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONBIO with null pointer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socket, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONREAD with null pointer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socket, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONREAD with null pointer must fail with EFAULT");

    void* const unmapped = reinterpret_cast<void*>(std::uintptr_t{0x10});
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socket, NonblockingRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONBIO with unmapped pointer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socket, NonblockingRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONBIO with unmapped pointer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socket, ReadableRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONREAD with unmapped pointer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socket, ReadableRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONREAD with unmapped pointer must fail with EFAULT");

    std::int64_t physical = 0;
    void* page = nullptr;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, 0x4000, 0, 0, &physical) == 0,
        "sceKernelAllocateDirectMemory failed");
    Require(sceKernelMapDirectMemory(&page, 0x4000, 3, 0, physical, 0) == 0,
        "sceKernelMapDirectMemory failed");
    Require(sceKernelMprotect(page, 0x4000, 1) == 0,
        "sceKernelMprotect to read-only failed");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socket, ReadableRequest, page) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONREAD into read-only page must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socket, ReadableRequest, page) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONREAD into read-only page must fail with EFAULT");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(page), 0x4000) == 0,
        "sceKernelMunmap failed");
    Require(sceKernelReleaseDirectMemory(physical, 0x4000) == 0,
        "sceKernelReleaseDirectMemory failed");
}

void AuditUnsupportedRequests(int inetSocket, int unixSocket) {
    char buffer[16] = {};
    for (const std::uint64_t malformed : MalformedRequests) {
        *__error_nid_postfix() = 0;
        Require(ioctl_nid_postfix(inetSocket, malformed, buffer) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
            "ioctl malformed request must fail with ENOTTY");
        *__error_nid_postfix() = 0;
        Require(_ioctl_nid_postfix(inetSocket, malformed, buffer) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
            "_ioctl malformed request must fail with ENOTTY");
        *__error_nid_postfix() = 0;
        Require(ioctl_nid_postfix(inetSocket, malformed, nullptr) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
            "ioctl malformed request with null buffer must fail with ENOTTY");
        *__error_nid_postfix() = 0;
        Require(_ioctl_nid_postfix(inetSocket, malformed, nullptr) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
            "_ioctl malformed request with null buffer must fail with ENOTTY");
    }

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, WindowSizeRequest, buffer) == -1 && *__error_nid_postfix() == ErrorAddressNotAvailable,
        "ioctl non-network group request on AF_INET must fail with EADDRNOTAVAIL");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(inetSocket, WindowSizeRequest, buffer) == -1 && *__error_nid_postfix() == ErrorAddressNotAvailable,
        "_ioctl non-network group request on AF_INET must fail with EADDRNOTAVAIL");

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(unixSocket, WindowSizeRequest, buffer) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
        "ioctl unsupported request on AF_UNIX must fail with ENOTTY");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(unixSocket, WindowSizeRequest, buffer) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
        "_ioctl unsupported request on AF_UNIX must fail with ENOTTY");

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, EmptyInputRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
        "ioctl EmptyInputRequest must fail with ENOTTY");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(inetSocket, EmptyInputRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
        "_ioctl EmptyInputRequest must fail with ENOTTY");

    int dummy = 0;
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, IntegerArgumentRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl IntegerArgumentRequest with null buffer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, IntegerArgumentRequest, &dummy) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
        "ioctl IntegerArgumentRequest must fail with ENOTTY");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(inetSocket, IntegerArgumentRequest, &dummy) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
        "_ioctl IntegerArgumentRequest must fail with ENOTTY");

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, UnsupportedNetGroupRequest, &dummy) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
        "ioctl unsupported net group request must fail with ENOTTY");
    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(inetSocket, UnsupportedNetGroupRequest, &dummy) == -1 && *__error_nid_postfix() == ErrorInappropriateIoctl,
        "_ioctl unsupported net group request must fail with ENOTTY");
}

void AuditSocketPairBuffers() {
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(UnixDomain, Stream, 0, pair) == 0, "socketpair failed");

    int queued = -1;
    Require(ioctl_nid_postfix(pair[0], ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD on empty socketpair end 0 must return 0");
    queued = -1;
    Require(_ioctl_nid_postfix(pair[0], ReadableRequest, &queued) == 0 && queued == 0,
        "_ioctl FIONREAD on empty socketpair end 0 must return 0");
    queued = -1;
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD on empty socketpair end 1 must return 0");
    queued = -1;
    Require(_ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == 0,
        "_ioctl FIONREAD on empty socketpair end 1 must return 0");

    int nonblocking = 1;
    Require(ioctl_nid_postfix(pair[1], NonblockingRequest, &nonblocking) == 0,
        "setting FIONBIO on socketpair end 1 failed");
    char receiveBuffer[64] = {};
    *__error_nid_postfix() = 0;
    Require(recv_nid_postfix(pair[1], receiveBuffer, sizeof(receiveBuffer), 0) == -1 && *__error_nid_postfix() == ErrorAgain,
        "recv on empty non-blocking socketpair must fail with EWOULDBLOCK");
    *__error_nid_postfix() = 0;
    Require(read_nid_postfix(pair[1], receiveBuffer, sizeof(receiveBuffer)) == -1 && *__error_nid_postfix() == ErrorAgain,
        "read on empty non-blocking socketpair must fail with EWOULDBLOCK");

    const char message[] = "socket ioctl payload buffer test";
    const int messageLength = static_cast<int>(sizeof(message));
    Require(send_nid_postfix(pair[0], message, messageLength, 0) == messageLength,
        "send on socketpair end 0 failed");

    queued = -1;
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == messageLength,
        "FIONREAD on loaded socketpair must report sent byte count");
    queued = -1;
    Require(_ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == messageLength,
        "_ioctl FIONREAD on loaded socketpair must report sent byte count");

    constexpr int partialSize = 12;
    Require(recv_nid_postfix(pair[1], receiveBuffer, partialSize, 0) == partialSize,
        "partial recv on socketpair failed");
    Require(std::memcmp(receiveBuffer, message, partialSize) == 0,
        "partial recv data mismatch");

    queued = -1;
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == messageLength - partialSize,
        "FIONREAD after partial recv must report remaining byte count");
    queued = -1;
    Require(_ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == messageLength - partialSize,
        "_ioctl FIONREAD after partial recv must report remaining byte count");

    const int remainingSize = messageLength - partialSize;
    Require(recv_nid_postfix(pair[1], receiveBuffer + partialSize, remainingSize, 0) == remainingSize,
        "reading remainder on socketpair failed");
    Require(std::strcmp(receiveBuffer, message) == 0,
        "full message data mismatch");

    queued = -1;
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD on drained socketpair must return 0");
    queued = -1;
    Require(_ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == 0,
        "_ioctl FIONREAD on drained socketpair must return 0");

    int blocking = 0;
    Require(_ioctl_nid_postfix(pair[1], NonblockingRequest, &blocking) == 0,
        "restoring blocking mode with _ioctl FIONBIO failed");

    Require(shutdown_nid_postfix(pair[1], 0) == 0,
        "shutdown read on socketpair failed");
    queued = -1;
    Require(ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD on shut socket must return 0");
    queued = -1;
    Require(_ioctl_nid_postfix(pair[1], ReadableRequest, &queued) == 0 && queued == 0,
        "_ioctl FIONREAD on shut socket must return 0");

    Require(close_nid_postfix(pair[0]) == 0, "close socketpair 0 failed");
    Require(close_nid_postfix(pair[1]) == 0, "close socketpair 1 failed");
}

void AuditTcpBuffers() {
    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "listener socket failed");

    int queued = -1;
    Require(ioctl_nid_postfix(listener, ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD on listener must return 0");
    int nonblocking = 1;
    Require(ioctl_nid_postfix(listener, NonblockingRequest, &nonblocking) == 0,
        "FIONBIO on listener failed");

    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(listener, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorAgain,
        "accept on non-blocking listener without pending connection must fail with EWOULDBLOCK");

    std::array<std::uint8_t, 16> address{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0, "bind failed");
    Require(listen_nid_postfix(listener, 1) == 0, "listen failed");
    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &addressSize) == 0, "getsockname failed");

    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0, "client socket failed");
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "connect failed");

    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0, "accept failed");

    queued = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD on empty accepted TCP socket must return 0");
    Require(ioctl_nid_postfix(server, NonblockingRequest, &nonblocking) == 0,
        "FIONBIO on server socket failed");

    char buffer[64] = {};
    *__error_nid_postfix() = 0;
    Require(read_nid_postfix(server, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == ErrorAgain,
        "read on empty non-blocking server must fail with EWOULDBLOCK");

    const char payload[] = "stream data for ioctl audit";
    const int payloadLength = static_cast<int>(sizeof(payload));
    Require(write_nid_postfix(client, payload, payloadLength) == payloadLength, "write on client failed");

    queued = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queued) == 0 && queued == payloadLength,
        "FIONREAD on loaded TCP socket must report payload size");
    queued = -1;
    Require(_ioctl_nid_postfix(server, ReadableRequest, &queued) == 0 && queued == payloadLength,
        "_ioctl FIONREAD on loaded TCP socket must report payload size");

    constexpr int chunk = 8;
    Require(read_nid_postfix(server, buffer, chunk) == chunk, "read partial chunk failed");
    Require(std::memcmp(buffer, payload, chunk) == 0, "partial chunk mismatch");

    queued = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queued) == 0 && queued == payloadLength - chunk,
        "FIONREAD after partial read must report remaining bytes");

    Require(read_nid_postfix(server, buffer + chunk, payloadLength - chunk) == payloadLength - chunk,
        "read remaining chunk failed");
    Require(std::strcmp(buffer, payload) == 0, "reassembled stream mismatch");

    queued = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD after consuming stream must return 0");

    Require(shutdown_nid_postfix(server, 0) == 0, "shutdown server read failed");
    queued = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD on shut server socket must return 0");

    Require(close_nid_postfix(client) == 0, "close client failed");
    Require(close_nid_postfix(server) == 0, "close server failed");
    Require(close_nid_postfix(listener) == 0, "close listener failed");
}

void AuditUdpBuffers() {
    const int udp = socket_nid_postfix(Inet, Datagram, 0);
    Require(udp >= 0, "udp socket failed");

    int queued = -1;
    Require(ioctl_nid_postfix(udp, ReadableRequest, &queued) == 0 && queued == 0,
        "FIONREAD on empty UDP socket must return 0");
    int nonblocking = 1;
    Require(ioctl_nid_postfix(udp, NonblockingRequest, &nonblocking) == 0,
        "FIONBIO on UDP socket failed");

    char buffer[16] = {};
    *__error_nid_postfix() = 0;
    Require(recv_nid_postfix(udp, buffer, sizeof(buffer), 0) == -1 && *__error_nid_postfix() == ErrorAgain,
        "recv on empty non-blocking UDP socket must fail with EWOULDBLOCK");

    int blocking = 0;
    Require(ioctl_nid_postfix(udp, NonblockingRequest, &blocking) == 0,
        "disabling FIONBIO on UDP socket failed");

    Require(close_nid_postfix(udp) == 0, "close udp failed");
}

}

int main() {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    AuditDescriptorBounds();

    const int inetSocket = socket_nid_postfix(Inet, Stream, 0);
    Require(inetSocket >= 0, "inetSocket failed");
    int unixPair[2] = {-1, -1};
    Require(socketpair_nid_postfix(UnixDomain, Stream, 0, unixPair) == 0, "socketpair failed");

    AuditInvalidPointers(inetSocket);
    AuditUnsupportedRequests(inetSocket, unixPair[0]);

    Require(close_nid_postfix(inetSocket) == 0, "close inetSocket failed");
    Require(close_nid_postfix(unixPair[0]) == 0, "close unixPair 0 failed");
    Require(close_nid_postfix(unixPair[1]) == 0, "close unixPair 1 failed");

    AuditSocketPairBuffers();
    AuditTcpBuffers();
    AuditUdpBuffers();

    return 0;
}
