#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace {

constexpr int GuestEnoent = 2;
constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;

constexpr int SceOk = 0;
constexpr int SceEnoent = static_cast<int>(0x80020002);
constexpr int SceEbadf = static_cast<int>(0x80020009);
constexpr int SceEinval = static_cast<int>(0x80020016);

constexpr std::int16_t EvfiltVnode = -4;

constexpr std::uint16_t EvAdd = 0x0001;
constexpr std::uint16_t EvDelete = 0x0002;
constexpr std::uint16_t EvEnable = 0x0004;
constexpr std::uint16_t EvDisable = 0x0008;
constexpr std::uint16_t EvOneshot = 0x0010;
constexpr std::uint16_t EvClear = 0x0020;
constexpr std::uint16_t EvReceipt = 0x0040;
constexpr std::uint16_t EvError = 0x4000;

constexpr std::uint32_t NoteDelete = 0x0001;
constexpr std::uint32_t NoteWrite = 0x0002;
constexpr std::uint32_t NoteExtend = 0x0004;
constexpr std::uint32_t NoteAttrib = 0x0008;
constexpr std::uint32_t NoteLink = 0x0010;
constexpr std::uint32_t NoteRename = 0x0020;

constexpr int ReadWrite = 0x0002;
constexpr int Create = 0x0200;
constexpr int AfUnix = 1;
constexpr int SockStream = 1;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

KernelEvent Change(std::uintptr_t ident, std::int16_t filter, std::uint16_t flags, std::uint32_t fflags = 0, std::intptr_t data = 0, void* udata = nullptr) {
    KernelEvent event{};
    event.ident = ident;
    event.filter = filter;
    event.flags = flags;
    event.fflags = fflags;
    event.data = data;
    event.udata = udata;
    return event;
}

int Apply(int kq, const KernelEvent& change) {
    return kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr);
}

void RequireFailure(int result, int expectedError) {
    Require(result == -1);
    Require(*__error_nid_postfix() == expectedError);
}

int Poll(int kq, KernelEvent* events, int count) {
    const KernelTimespec zero{0, 0};
    return kevent_nid_postfix(kq, nullptr, 0, events, count, &zero);
}

int Wait(int kq, KernelEvent* events, int count, int seconds = 2) {
    const KernelTimespec timeout{seconds, 0};
    return kevent_nid_postfix(kq, nullptr, 0, events, count, &timeout);
}

std::string UniquePath(const char* label) {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path() /
        ("anyps5-vnode-" + std::to_string(stamp) + "-" + std::to_string(++counter) + "-" + label)).string();
}

void TestInvalidFdFilterRejection() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(-1), EvfiltVnode, EvAdd, NoteWrite)), GuestEbadf);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(-50), EvfiltVnode, EvAdd, NoteDelete)), GuestEbadf);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(INT_MAX) + 1ULL, EvfiltVnode, EvAdd, NoteWrite)), GuestEbadf);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(UINT64_MAX), EvfiltVnode, EvAdd, NoteDelete)), GuestEbadf);
    RequireFailure(Apply(kq, Change(65530, EvfiltVnode, EvAdd, NoteWrite)), GuestEbadf);
    RequireFailure(Apply(kq, Change(98765, EvfiltVnode, EvAdd, NoteDelete)), GuestEbadf);

    const auto tempPath = UniquePath("invalid_fd_probe");
    const int validFd = open_nid_postfix(tempPath.c_str(), ReadWrite | Create, 0644);
    Require(validFd >= 0);
    Require(close_nid_postfix(validFd) == 0);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(validFd), EvfiltVnode, EvAdd, NoteWrite)), GuestEbadf);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(validFd), EvfiltVnode, EvDelete)), GuestEbadf);
    std::filesystem::remove(tempPath);

    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(kq), EvfiltVnode, EvAdd, NoteWrite)), GuestEinval);

    int sockets[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, sockets) == 0);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(sockets[0]), EvfiltVnode, EvAdd, NoteWrite)), GuestEinval);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(sockets[1]), EvfiltVnode, EvAdd, NoteDelete)), GuestEinval);
    Require(close_nid_postfix(sockets[0]) == 0);
    Require(close_nid_postfix(sockets[1]) == 0);

    int pipes[2]{-1, -1};
    Require(pipe_nid_postfix(pipes) == 0);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(pipes[0]), EvfiltVnode, EvAdd, NoteWrite)), GuestEinval);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(pipes[1]), EvfiltVnode, EvAdd, NoteDelete)), GuestEinval);
    Require(close_nid_postfix(pipes[0]) == 0);
    Require(close_nid_postfix(pipes[1]) == 0);

    KernelEvent changeReceipt = Change(77771, EvfiltVnode, EvAdd | EvReceipt, NoteWrite);
    KernelEvent outEvent{};
    Require(kevent_nid_postfix(kq, &changeReceipt, 1, &outEvent, 1, nullptr) == 1);
    Require((outEvent.flags & EvError) != 0);
    Require(outEvent.data == GuestEbadf);

    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, sockets) == 0);
    changeReceipt = Change(static_cast<std::uintptr_t>(sockets[0]), EvfiltVnode, EvAdd | EvReceipt, NoteWrite);
    Require(kevent_nid_postfix(kq, &changeReceipt, 1, &outEvent, 1, nullptr) == 1);
    Require((outEvent.flags & EvError) != 0);
    Require(outEvent.data == GuestEinval);
    Require(close_nid_postfix(sockets[0]) == 0);
    Require(close_nid_postfix(sockets[1]) == 0);

    const KernelEvent anyChange = Change(1, EvfiltVnode, EvAdd, NoteWrite);
    RequireFailure(kevent_nid_postfix(-1, &anyChange, 1, nullptr, 0, nullptr), GuestEbadf);
    RequireFailure(kevent_nid_postfix(99999, &anyChange, 1, nullptr, 0, nullptr), GuestEbadf);

    const auto openPath = UniquePath("open_bounds_probe");
    const int liveFd = open_nid_postfix(openPath.c_str(), ReadWrite | Create, 0644);
    Require(liveFd >= 0);
    const auto liveIdent = static_cast<std::uintptr_t>(liveFd);

    RequireFailure(Apply(kq, Change(liveIdent, EvfiltVnode, EvEnable | EvDisable, NoteWrite)), GuestEinval);
    RequireFailure(Apply(kq, Change(liveIdent, 1, EvAdd, NoteWrite)), GuestEinval);
    RequireFailure(Apply(kq, Change(liveIdent, EvfiltVnode, EvDelete)), GuestEnoent);

    Require(close_nid_postfix(liveFd) == 0);
    std::filesystem::remove(openPath);
    Require(close_nid_postfix(kq) == 0);
}

void TestVnodeNoteWrite() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const auto path = UniquePath("note_write_file");
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const auto ident = static_cast<std::uintptr_t>(file);

    int marker = 42;
    KernelEvent events[4]{};

    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteWrite, 0, &marker)) == 0);
    Require(Poll(kq, events, 4) == 0);

    Require(write_nid_postfix(file, "chunk_alpha", 11) == 11);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident);
    Require(events[0].filter == EvfiltVnode);
    Require((events[0].fflags & NoteWrite) != 0);
    Require(events[0].udata == &marker);
    Require((events[0].flags & EvClear) != 0);
    Require(Poll(kq, events, 4) == 0);

    Require(write_nid_postfix(file, "chunk_beta", 10) == 10);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident);
    Require((events[0].fflags & NoteWrite) != 0);
    Require(Poll(kq, events, 4) == 0);

    Require(lseek_nid_postfix(file, 0, 0) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(write_nid_postfix(file, "overwrite!!", 11) == 11);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident);
    Require((events[0].fflags & NoteWrite) != 0);
    Require(Poll(kq, events, 4) == 0);

    Require(Apply(kq, Change(ident, EvfiltVnode, EvDisable, NoteWrite)) == 0);
    Require(write_nid_postfix(file, "ignored", 7) == 7);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(Poll(kq, events, 4) == 0);

    Require(Apply(kq, Change(ident, EvfiltVnode, EvEnable, NoteWrite)) == 0);
    Require(write_nid_postfix(file, "resumed", 7) == 7);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident);
    Require((events[0].fflags & NoteWrite) != 0);

    Require(Apply(kq, Change(ident, EvfiltVnode, EvDelete)) == 0);
    RequireFailure(Apply(kq, Change(ident, EvfiltVnode, EvDelete)), GuestEnoent);

    Require(write_nid_postfix(file, "post_delete", 11) == 11);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(Poll(kq, events, 4) == 0);

    Require(close_nid_postfix(file) == 0);
    Require(unlink_nid_postfix(path.c_str()) == 0);
    Require(close_nid_postfix(kq) == 0);
}

void TestVnodeNoteDelete() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const auto path = UniquePath("note_delete_file");
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const auto ident = static_cast<std::uintptr_t>(file);

    int marker = 99;
    KernelEvent events[4]{};

    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteDelete, 0, &marker)) == 0);
    Require(Poll(kq, events, 4) == 0);

    Require(write_nid_postfix(file, "content", 7) == 7);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(Poll(kq, events, 4) == 0);

    Require(unlink_nid_postfix(path.c_str()) == 0);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident);
    Require(events[0].filter == EvfiltVnode);
    Require((events[0].fflags & NoteDelete) != 0);
    Require(events[0].udata == &marker);

    Require(close_nid_postfix(file) == 0);
    Require(close_nid_postfix(kq) == 0);
}

void TestVnodeCombinedEvents() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const auto path = UniquePath("combined_events_file");
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const auto ident = static_cast<std::uintptr_t>(file);

    KernelEvent events[4]{};
    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteWrite | NoteDelete)) == 0);
    Require(Poll(kq, events, 4) == 0);

    Require(write_nid_postfix(file, "multi_step", 10) == 10);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident);
    Require((events[0].fflags & NoteWrite) != 0);
    Require(Poll(kq, events, 4) == 0);

    Require(unlink_nid_postfix(path.c_str()) == 0);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident);
    Require((events[0].fflags & NoteDelete) != 0);

    Require(close_nid_postfix(file) == 0);
    Require(close_nid_postfix(kq) == 0);
}

void TestVnodeOneshot() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const auto path = UniquePath("oneshot_file");
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const auto ident = static_cast<std::uintptr_t>(file);

    KernelEvent events[4]{};
    int marker = 77;
    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvOneshot, NoteWrite, 0, &marker)) == 0);
    Require(Poll(kq, events, 4) == 0);

    Require(write_nid_postfix(file, "single_shot", 11) == 11);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident);
    Require((events[0].flags & EvOneshot) != 0);
    Require((events[0].fflags & NoteWrite) != 0);
    Require(events[0].udata == &marker);

    Require(write_nid_postfix(file, "next_shot", 9) == 9);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(Poll(kq, events, 4) == 0);

    RequireFailure(Apply(kq, Change(ident, EvfiltVnode, EvDelete)), GuestEnoent);

    Require(close_nid_postfix(file) == 0);
    Require(unlink_nid_postfix(path.c_str()) == 0);
    Require(close_nid_postfix(kq) == 0);
}

void TestDescriptorCloseDeregistration() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0);

    const auto path = UniquePath("close_dereg_file");
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const auto ident = static_cast<std::uintptr_t>(file);

    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteWrite)) == 0);
    Require(close_nid_postfix(file) == 0);

    RequireFailure(Apply(kq, Change(ident, EvfiltVnode, EvDelete)), GuestEbadf);

    Require(unlink_nid_postfix(path.c_str()) == 0);
    Require(close_nid_postfix(kq) == 0);
}

void TestSceKernelFileEventApi() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "file_event_audit") == SceOk);

    Require(sceKernelAddFileEvent(eq, -1, static_cast<int>(NoteWrite), nullptr) == SceEbadf);
    Require(sceKernelAddFileEvent(eq, 99995, static_cast<int>(NoteWrite), nullptr) == SceEbadf);
    Require(sceKernelDeleteFileEvent(eq, -1) == SceEbadf);
    Require(sceKernelDeleteFileEvent(eq, 99995) == SceEbadf);

    int sockets[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, sockets) == 0);
    Require(sceKernelAddFileEvent(eq, sockets[0], static_cast<int>(NoteWrite), nullptr) == SceEinval);
    Require(close_nid_postfix(sockets[0]) == 0);
    Require(close_nid_postfix(sockets[1]) == 0);

    const auto path = UniquePath("sce_file_event");
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const auto ident = static_cast<std::uintptr_t>(file);

    int marker = 123;
    Require(sceKernelAddFileEvent(eq, file, static_cast<int>(NoteWrite), &marker) == SceOk);

    Require(write_nid_postfix(file, "sce_event_data", 14) == 14);

    KernelEvent events[4]{};
    int count = 0;
    const KernelUseconds timeout = 2000000;
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &timeout) == SceOk && count == 1);
    Require(events[0].ident == ident);
    Require(events[0].filter == EvfiltVnode);
    Require((events[0].fflags & NoteWrite) != 0);
    Require(events[0].udata == &marker);

    Require(sceKernelDeleteFileEvent(eq, file) == SceOk);
    Require(sceKernelDeleteFileEvent(eq, file) == SceEnoent);

    Require(close_nid_postfix(file) == 0);
    Require(unlink_nid_postfix(path.c_str()) == 0);
    Require(sceKernelDeleteEqueue(eq) == SceOk);
}

}

int main() {
    TestInvalidFdFilterRejection();
    TestVnodeNoteWrite();
    TestVnodeNoteDelete();
    TestVnodeCombinedEvents();
    TestVnodeOneshot();
    TestDescriptorCloseDeregistration();
    TestSceKernelFileEventApi();
    std::puts("guest_kernel_kqueue_vnode_events tests passed");
    return 0;
}
