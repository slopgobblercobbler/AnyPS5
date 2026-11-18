#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int NeverOpenedHandle = 2;
constexpr int VideoOutBufferIndexBlack = -2;
constexpr int VideoOutBufferIndexBlank = -1;
constexpr int VideoOutFlipModeVsync = 1;
constexpr std::size_t FlipQueueCapacity = 16;
constexpr int VideoOutErrorInvalidValue = -2144796671;
constexpr int VideoOutErrorInvalidIndex = -2144796662;
constexpr int VideoOutErrorInvalidAddress = -2144796670;
constexpr int VideoOutErrorFlipQueueFull = -2144796654;
constexpr int VideoOutErrorInvalidHandle = -2144796661;
constexpr int VideoOutErrorInvalidEventQueue = -2144796660;
constexpr int VideoOutErrorInvalidEvent = -2144796659;

void Require(bool condition) {
    if (!condition) std::abort();
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestUnopenedHandleBounds() {
    const int badHandles[] = {-100, -1, 0, NeverOpenedHandle, 9999};
    for (const int badHandle : badHandles) {
        Require(Throws([badHandle] {
            return sceVideoOutSubmitFlip(badHandle, VideoOutBufferIndexBlack, VideoOutFlipModeVsync, 0);
        }));

        std::uint32_t token = 0x5a5a5a5au;
        Require(sceVideoOutSubmitEopFlip(badHandle, VideoOutBufferIndexBlack, VideoOutFlipModeVsync, 0, &token) == VideoOutErrorInvalidHandle);
        Require(token == 0x5a5a5a5au);

        Require(sceVideoOutSubmitEopFlip(badHandle, VideoOutBufferIndexBlack, VideoOutFlipModeVsync, 0, nullptr) == VideoOutErrorInvalidAddress);

        VideoOutFlipStatus status{};
        Require(sceVideoOutGetFlipStatus(badHandle, &status) == VideoOutErrorInvalidHandle);

        Require(Throws([badHandle] {
            return sceVideoOutGetFlipStatus(badHandle, nullptr);
        }));

        Require(sceVideoOutIsFlipPending(badHandle) == VideoOutErrorInvalidHandle);

        Require(sceVideoOutAddFlipEvent(0, badHandle, nullptr) == VideoOutErrorInvalidHandle);
        Require(sceVideoOutDeleteFlipEvent(0, badHandle) == VideoOutErrorInvalidHandle);
    }
}

void TestOpenHandleParameterBounds(int handle) {
    const int badModes[] = {-10, -1, 0, 7, 8, 100};
    for (const int badMode : badModes) {
        Require(Throws([handle, badMode] {
            return sceVideoOutSubmitFlip(handle, VideoOutBufferIndexBlack, badMode, 0);
        }));

        std::uint32_t token = 0x5a5a5a5au;
        Require(sceVideoOutSubmitEopFlip(handle, VideoOutBufferIndexBlack, badMode, 0, &token) == VideoOutErrorInvalidValue);
        Require(token == 0x5a5a5a5au);
    }

    const int badIndices[] = {-100, -5, -3, 16, 17, 32, 100};
    for (const int badIndex : badIndices) {
        Require(Throws([handle, badIndex] {
            return sceVideoOutSubmitFlip(handle, badIndex, VideoOutFlipModeVsync, 0);
        }));

        std::uint32_t token = 0x5a5a5a5au;
        Require(sceVideoOutSubmitEopFlip(handle, badIndex, VideoOutFlipModeVsync, 0, &token) == VideoOutErrorInvalidIndex);
        Require(token == 0x5a5a5a5au);
    }

    for (int unregisteredIndex = 0; unregisteredIndex < 16; ++unregisteredIndex) {
        Require(Throws([handle, unregisteredIndex] {
            return sceVideoOutSubmitFlip(handle, unregisteredIndex, VideoOutFlipModeVsync, 0);
        }));

        std::uint32_t token = 0x5a5a5a5au;
        Require(sceVideoOutSubmitEopFlip(handle, unregisteredIndex, VideoOutFlipModeVsync, 0, &token) == VideoOutErrorInvalidIndex);
        Require(token == 0x5a5a5a5au);
    }

    Require(sceVideoOutSubmitEopFlip(handle, VideoOutBufferIndexBlack, VideoOutFlipModeVsync, 0, nullptr) == VideoOutErrorInvalidAddress);
}

void TestGetFlipStatusAndPendingBounds(int handle) {
    Require(sceVideoOutGetFlipStatus(handle, nullptr) == VideoOutErrorInvalidAddress);

    VideoOutFlipStatus status{};
    Require(sceVideoOutGetFlipStatus(handle, &status) == 0);
    Require(status.count == 0);
    Require(status.flipPendingNum == 0);
    Require(status.currentBuffer == -1);
    Require(sceVideoOutIsFlipPending(handle) == 0);
}

void TestFlipQueueCapacityAndOverflowBounds(int handle) {
    std::array<std::uint32_t, FlipQueueCapacity> tokens{};
    for (std::size_t i = 0; i < FlipQueueCapacity; ++i) {
        tokens[i] = 0;
        const int index = (i % 2 == 0) ? VideoOutBufferIndexBlack : VideoOutBufferIndexBlank;
        Require(sceVideoOutSubmitEopFlip(handle, index, VideoOutFlipModeVsync, static_cast<std::int64_t>(i + 1), &tokens[i]) == 0);
        Require(tokens[i] != 0);
        Require(sceVideoOutIsFlipPending(handle) == static_cast<int>(i + 1));

        VideoOutFlipStatus status{};
        Require(sceVideoOutGetFlipStatus(handle, &status) == 0);
        Require(status.flipPendingNum == static_cast<std::int32_t>(i + 1));
    }

    for (std::size_t i = 0; i < FlipQueueCapacity; ++i) {
        for (std::size_t j = i + 1; j < FlipQueueCapacity; ++j) {
            Require(tokens[i] != tokens[j]);
        }
    }

    std::uint32_t overflowToken = 0xDEADBEEFu;
    Require(sceVideoOutSubmitEopFlip(handle, VideoOutBufferIndexBlack, VideoOutFlipModeVsync, 999, &overflowToken) == VideoOutErrorFlipQueueFull);
    Require(overflowToken == 0xDEADBEEFu);

    overflowToken = 0xCAFEBABEu;
    Require(sceVideoOutSubmitEopFlip(handle, VideoOutBufferIndexBlank, VideoOutFlipModeVsync, 1000, &overflowToken) == VideoOutErrorFlipQueueFull);
    Require(overflowToken == 0xCAFEBABEu);

    Require(sceVideoOutIsFlipPending(handle) == static_cast<int>(FlipQueueCapacity));
    VideoOutFlipStatus status{};
    Require(sceVideoOutGetFlipStatus(handle, &status) == 0);
    Require(status.flipPendingNum == static_cast<std::int32_t>(FlipQueueCapacity));
}

void TestFlipEventQueueIntegration(int handle) {
    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "test flip queue") == 0);
    Require(queue != 0);

    Require(sceVideoOutDeleteFlipEvent(queue, handle) == VideoOutErrorInvalidEvent);

    Require(sceVideoOutAddFlipEvent(queue, handle, nullptr) == 0);

    int tag = 42;
    Require(sceVideoOutAddFlipEvent(queue, handle, &tag) == 0);

    Require(sceVideoOutAddFlipEvent(0, handle, nullptr) == VideoOutErrorInvalidEventQueue);
    Require(sceVideoOutDeleteFlipEvent(0, handle) == VideoOutErrorInvalidEventQueue);

    Require(sceVideoOutDeleteFlipEvent(queue, handle) == 0);
    Require(sceVideoOutDeleteFlipEvent(queue, handle) == VideoOutErrorInvalidEvent);

    Require(sceKernelDeleteEqueue(queue) == 0);
}

void TestCloseAndReopenLifecycle(int handle) {
    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == VideoOutErrorInvalidHandle);

    Require(Throws([handle] {
        return sceVideoOutSubmitFlip(handle, VideoOutBufferIndexBlack, VideoOutFlipModeVsync, 0);
    }));

    std::uint32_t token = 0x5a5a5a5au;
    Require(sceVideoOutSubmitEopFlip(handle, VideoOutBufferIndexBlack, VideoOutFlipModeVsync, 0, &token) == VideoOutErrorInvalidHandle);
    Require(token == 0x5a5a5a5au);

    VideoOutFlipStatus status{};
    Require(sceVideoOutGetFlipStatus(handle, &status) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutIsFlipPending(handle) == VideoOutErrorInvalidHandle);

    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "test post-close queue") == 0);
    Require(sceVideoOutAddFlipEvent(queue, handle, nullptr) == VideoOutErrorInvalidHandle);
    Require(sceVideoOutDeleteFlipEvent(queue, handle) == VideoOutErrorInvalidHandle);
    Require(sceKernelDeleteEqueue(queue) == 0);

    int reopened = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    Require(reopened > 0);
    Require(sceVideoOutIsFlipPending(reopened) == 0);

    VideoOutFlipStatus freshStatus{};
    Require(sceVideoOutGetFlipStatus(reopened, &freshStatus) == 0);
    Require(freshStatus.count == 0);
    Require(freshStatus.flipPendingNum == 0);

    std::uint32_t freshToken = 0;
    Require(sceVideoOutSubmitEopFlip(reopened, VideoOutBufferIndexBlack, VideoOutFlipModeVsync, 1, &freshToken) == 0);
    Require(freshToken != 0);
    Require(sceVideoOutIsFlipPending(reopened) == 1);

    Require(sceVideoOutClose(reopened) == 0);
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestUnopenedHandleBounds();

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    TestOpenHandleParameterBounds(handle);
    TestGetFlipStatusAndPendingBounds(handle);
    TestFlipQueueCapacityAndOverflowBounds(handle);
    TestFlipEventQueueIntegration(handle);
    TestCloseAndReopenLifecycle(handle);

    LibcRunShutdown_nid_postfix();
    return 0;
}
