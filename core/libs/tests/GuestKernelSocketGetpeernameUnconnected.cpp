#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int FamilyInet = 2;
constexpr int FamilyInet6 = 28;
constexpr int TypeStream = 1;
constexpr int TypeDatagram = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorAddressNotAvailable = 49;
constexpr int ErrorAddressFamilyNotSupported = 47;
constexpr int ErrorNotConnected = 57;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

bool Fails(long result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

std::array<std::uint8_t, 16> LoopbackAddress(std::uint16_t port = 0) {
    return {16, FamilyInet, static_cast<std::uint8_t>(port >> 8), static_cast<std::uint8_t>(port), 127, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
}

std::array<std::uint8_t, 28> Loopback6Address(std::uint16_t port = 0) {
    std::array<std::uint8_t, 28> address{28, FamilyInet6, static_cast<std::uint8_t>(port >> 8), static_cast<std::uint8_t>(port)};
    address[23] = 1;
    return address;
}

void AuditNullLengthFaultRejection() {
    std::array<std::uint8_t, 16> buffer{};

    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(-1, buffer.data(), nullptr), ErrorFault),
        "getpeername on -1 with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(_getpeername_nid_postfix(-1, buffer.data(), nullptr), ErrorFault),
        "_getpeername on -1 with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(-1, buffer.data(), nullptr), ErrorFault),
        "getsockname on -1 with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(_getsockname_nid_postfix(-1, buffer.data(), nullptr), ErrorFault),
        "_getsockname on -1 with null length must fail with EFAULT (14)");

    const int udp = socket_nid_postfix(FamilyInet, TypeDatagram, 0);
    Require(udp >= 0, "socket UDP");
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(udp, buffer.data(), nullptr), ErrorFault),
        "getpeername on unconnected UDP with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(_getpeername_nid_postfix(udp, buffer.data(), nullptr), ErrorFault),
        "_getpeername on unconnected UDP with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(udp, buffer.data(), nullptr), ErrorFault),
        "getsockname on unconnected UDP with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(_getsockname_nid_postfix(udp, buffer.data(), nullptr), ErrorFault),
        "_getsockname on unconnected UDP with null length must fail with EFAULT (14)");

    const int tcp = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(tcp >= 0, "socket TCP");
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(tcp, buffer.data(), nullptr), ErrorFault),
        "getpeername on unconnected TCP with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(_getpeername_nid_postfix(tcp, buffer.data(), nullptr), ErrorFault),
        "_getpeername on unconnected TCP with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(tcp, buffer.data(), nullptr), ErrorFault),
        "getsockname on unconnected TCP with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(_getsockname_nid_postfix(tcp, buffer.data(), nullptr), ErrorFault),
        "_getsockname on unconnected TCP with null length must fail with EFAULT (14)");

    int pipes[2]{-1, -1};
    Require(pipe_nid_postfix(pipes) == 0, "pipe");
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(pipes[0], buffer.data(), nullptr), ErrorFault),
        "getpeername on pipe with null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(pipes[0], buffer.data(), nullptr), ErrorFault),
        "getsockname on pipe with null length must fail with EFAULT (14)");

    Require(close_nid_postfix(pipes[0]) == 0 && close_nid_postfix(pipes[1]) == 0, "close pipe");
    Require(close_nid_postfix(udp) == 0, "close udp");
    Require(close_nid_postfix(tcp) == 0, "close tcp");
}

void AuditDescriptorCheckPrecedence() {
    std::array<std::uint8_t, 16> buffer{};
    std::uint32_t length = buffer.size();

    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(-1, buffer.data(), &length), ErrorBadDescriptor),
        "getpeername(-1) with valid length must fail with EBADF (9)");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(-1, buffer.data(), &length), ErrorBadDescriptor),
        "getsockname(-1) with valid length must fail with EBADF (9)");

    int pipes[2]{-1, -1};
    Require(pipe_nid_postfix(pipes) == 0, "pipe");
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(pipes[0], buffer.data(), &length), ErrorNotSocket),
        "getpeername on pipe with valid length must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(pipes[0], buffer.data(), &length), ErrorNotSocket),
        "getsockname on pipe with valid length must fail with ENOTSOCK (38)");
    Require(close_nid_postfix(pipes[0]) == 0 && close_nid_postfix(pipes[1]) == 0, "close pipe");
}

void AuditUnconnectedUdpSocket() {
    const int sock = socket_nid_postfix(FamilyInet, TypeDatagram, 0);
    Require(sock >= 0, "socket UDP");

    std::array<std::uint8_t, 16> localAddress{};
    std::uint32_t localLength = localAddress.size();
    Require(getsockname_nid_postfix(sock, localAddress.data(), &localLength) == 0,
        "getsockname on fresh unbound UDP socket must succeed");
    Require(localLength == localAddress.size(), "getsockname returned length must match sockaddr_in");

    std::array<std::uint8_t, 16> peerAddress{};
    std::uint32_t peerLength = peerAddress.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, peerAddress.data(), &peerLength), ErrorNotConnected),
        "getpeername on fresh unbound UDP socket must fail with ENOTCONN (57)");
    *__error_nid_postfix() = 0;
    Require(Fails(_getpeername_nid_postfix(sock, peerAddress.data(), &peerLength), ErrorNotConnected),
        "_getpeername on fresh unbound UDP socket must fail with ENOTCONN (57)");

    const auto bindAddress = LoopbackAddress(0);
    Require(bind_nid_postfix(sock, bindAddress.data(), bindAddress.size()) == 0, "bind UDP socket");

    localLength = localAddress.size();
    Require(getsockname_nid_postfix(sock, localAddress.data(), &localLength) == 0,
        "getsockname on bound UDP socket must succeed");
    const std::uint16_t boundPort = (static_cast<std::uint16_t>(localAddress[2]) << 8) | localAddress[3];
    Require(boundPort != 0, "bound UDP socket must receive an assigned ephemeral port");

    peerLength = peerAddress.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, peerAddress.data(), &peerLength), ErrorNotConnected),
        "getpeername on bound unconnected UDP socket must fail with ENOTCONN (57)");

    const int peer = socket_nid_postfix(FamilyInet, TypeDatagram, 0);
    Require(peer >= 0, "socket peer UDP");
    const auto peerBind = LoopbackAddress(0);
    Require(bind_nid_postfix(peer, peerBind.data(), peerBind.size()) == 0, "bind peer UDP socket");

    std::array<std::uint8_t, 16> targetAddress{};
    std::uint32_t targetLength = targetAddress.size();
    Require(getsockname_nid_postfix(peer, targetAddress.data(), &targetLength) == 0, "getsockname on target peer UDP");

    Require(connect_nid_postfix(sock, targetAddress.data(), targetLength) == 0, "connect UDP socket to peer");

    peerLength = peerAddress.size();
    Require(getpeername_nid_postfix(sock, peerAddress.data(), &peerLength) == 0,
        "getpeername on connected UDP socket must succeed");
    Require(peerLength == targetLength, "peerLength must match connected target length");
    Require(std::memcmp(peerAddress.data(), targetAddress.data(), peerLength) == 0,
        "peer address must match target address on connected UDP socket");

    const auto portZeroAddress = LoopbackAddress(0);
    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(sock, portZeroAddress.data(), portZeroAddress.size()), ErrorAddressNotAvailable),
        "connect to port 0 on connected UDP socket must fail with EADDRNOTAVAIL (49)");

    peerLength = peerAddress.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, peerAddress.data(), &peerLength), ErrorNotConnected),
        "getpeername after port 0 disconnect must fail with ENOTCONN (57)");

    localLength = localAddress.size();
    Require(getsockname_nid_postfix(sock, localAddress.data(), &localLength) == 0,
        "getsockname on disconnected UDP socket must succeed");

    std::uint32_t zeroLength = 0;
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, peerAddress.data(), &zeroLength), ErrorNotConnected),
        "getpeername with zero capacity on unconnected UDP socket must fail with ENOTCONN (57)");

    zeroLength = 0;
    Require(getsockname_nid_postfix(sock, peerAddress.data(), &zeroLength) == 0,
        "getsockname with zero capacity on UDP socket must succeed");
    Require(zeroLength == 16, "getsockname with zero capacity must update length to 16");

    Require(close_nid_postfix(peer) == 0, "close peer");
    Require(close_nid_postfix(sock) == 0, "close sock");
}

void AuditUnconnectedTcpSocket() {
    const int sock = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(sock >= 0, "socket TCP");

    std::array<std::uint8_t, 16> localAddress{};
    std::uint32_t localLength = localAddress.size();
    Require(getsockname_nid_postfix(sock, localAddress.data(), &localLength) == 0,
        "getsockname on fresh unbound TCP socket must succeed");
    Require(localLength == localAddress.size(), "getsockname returned length must match sockaddr_in");

    std::array<std::uint8_t, 16> peerAddress{};
    std::uint32_t peerLength = peerAddress.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, peerAddress.data(), &peerLength), ErrorNotConnected),
        "getpeername on fresh unbound TCP socket must fail with ENOTCONN (57)");
    *__error_nid_postfix() = 0;
    Require(Fails(_getpeername_nid_postfix(sock, peerAddress.data(), &peerLength), ErrorNotConnected),
        "_getpeername on fresh unbound TCP socket must fail with ENOTCONN (57)");

    const auto bindAddress = LoopbackAddress(0);
    Require(bind_nid_postfix(sock, bindAddress.data(), bindAddress.size()) == 0, "bind TCP listener");

    localLength = localAddress.size();
    Require(getsockname_nid_postfix(sock, localAddress.data(), &localLength) == 0,
        "getsockname on bound TCP socket must succeed");

    peerLength = peerAddress.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, peerAddress.data(), &peerLength), ErrorNotConnected),
        "getpeername on bound unconnected TCP socket must fail with ENOTCONN (57)");

    Require(listen_nid_postfix(sock, 1) == 0, "listen TCP socket");

    peerLength = peerAddress.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, peerAddress.data(), &peerLength), ErrorNotConnected),
        "getpeername on listening TCP socket must fail with ENOTCONN (57)");

    const int client = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(client >= 0, "socket client TCP");

    peerLength = peerAddress.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(client, peerAddress.data(), &peerLength), ErrorNotConnected),
        "getpeername on fresh client TCP socket must fail with ENOTCONN (57)");

    Require(connect_nid_postfix(client, localAddress.data(), localLength) == 0, "connect client TCP to listener");

    const int accepted = accept_nid_postfix(sock, nullptr, nullptr);
    Require(accepted >= 0, "accept connected TCP client");

    peerLength = peerAddress.size();
    Require(getpeername_nid_postfix(client, peerAddress.data(), &peerLength) == 0,
        "getpeername on connected client TCP must succeed");
    Require(peerLength == localLength, "client peerLength must match listener address length");
    Require(std::memcmp(peerAddress.data(), localAddress.data(), peerLength) == 0,
        "client peer address must match listener local address");

    std::array<std::uint8_t, 16> clientLocal{};
    std::uint32_t clientLocalLength = clientLocal.size();
    Require(getsockname_nid_postfix(client, clientLocal.data(), &clientLocalLength) == 0,
        "getsockname on connected client TCP must succeed");

    peerLength = peerAddress.size();
    Require(getpeername_nid_postfix(accepted, peerAddress.data(), &peerLength) == 0,
        "getpeername on accepted server TCP must succeed");
    Require(peerLength == clientLocalLength, "accepted peerLength must match client address length");
    Require(std::memcmp(peerAddress.data(), clientLocal.data(), peerLength) == 0,
        "accepted peer address must match client local address");

    Require(close_nid_postfix(accepted) == 0, "close accepted");
    Require(close_nid_postfix(client) == 0, "close client");
    Require(close_nid_postfix(sock) == 0, "close listener");
}

void AuditUnconnectedIpv6Sockets() {
    const int udp6 = socket_nid_postfix(FamilyInet6, TypeDatagram, 0);
    if (udp6 < 0 && *__error_nid_postfix() == ErrorAddressFamilyNotSupported) return;
    Require(udp6 >= 0, "socket IPv6 UDP");

    std::array<std::uint8_t, 28> local6{};
    std::uint32_t localLength = local6.size();
    Require(getsockname_nid_postfix(udp6, local6.data(), &localLength) == 0,
        "getsockname on unbound IPv6 UDP socket must succeed");
    Require(localLength == local6.size(), "getsockname returned length must match sockaddr_in6");

    std::array<std::uint8_t, 28> peer6{};
    std::uint32_t peerLength = peer6.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(udp6, peer6.data(), &peerLength), ErrorNotConnected),
        "getpeername on unconnected IPv6 UDP socket must fail with ENOTCONN (57)");

    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(udp6, peer6.data(), nullptr), ErrorFault),
        "getpeername on unconnected IPv6 UDP with null length must fail with EFAULT (14)");

    const auto bind6 = Loopback6Address(0);
    const int bound = bind_nid_postfix(udp6, bind6.data(), bind6.size());
    if (bound == 0) {
        localLength = local6.size();
        Require(getsockname_nid_postfix(udp6, local6.data(), &localLength) == 0,
            "getsockname on bound IPv6 UDP socket must succeed");
        peerLength = peer6.size();
        *__error_nid_postfix() = 0;
        Require(Fails(getpeername_nid_postfix(udp6, peer6.data(), &peerLength), ErrorNotConnected),
            "getpeername on bound unconnected IPv6 UDP socket must fail with ENOTCONN (57)");
    }
    Require(close_nid_postfix(udp6) == 0, "close IPv6 UDP");

    const int tcp6 = socket_nid_postfix(FamilyInet6, TypeStream, 0);
    if (tcp6 < 0 && *__error_nid_postfix() == ErrorAddressFamilyNotSupported) return;
    Require(tcp6 >= 0, "socket IPv6 TCP");

    localLength = local6.size();
    Require(getsockname_nid_postfix(tcp6, local6.data(), &localLength) == 0,
        "getsockname on unbound IPv6 TCP socket must succeed");
    peerLength = peer6.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(tcp6, peer6.data(), &peerLength), ErrorNotConnected),
        "getpeername on unconnected IPv6 TCP socket must fail with ENOTCONN (57)");

    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(tcp6, peer6.data(), nullptr), ErrorFault),
        "getpeername on unconnected IPv6 TCP with null length must fail with EFAULT (14)");

    Require(close_nid_postfix(tcp6) == 0, "close IPv6 TCP");
}

}

int main() {
    AuditNullLengthFaultRejection();
    AuditDescriptorCheckPrecedence();
    AuditUnconnectedUdpSocket();
    AuditUnconnectedTcpSocket();
    AuditUnconnectedIpv6Sockets();
    return 0;
}
