#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int SCE_VIDEO_OUT_ERROR_INVALID_VALUE = static_cast<int>(0x80290001);

static void Require(bool value) {
    if (!value) std::abort();
}

static void ExpectFailure(auto&& action) {
    bool failed = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(failed);
}

int main() {
    VideoOutTestEnvironment environment;

    ExpectFailure([] {
        sceVideoOutSetBufferAttribute(nullptr, 0x80000000u, 1, 0, 1920, 1080, 1920);
    });

    VideoOutBufferAttribute attribute{};
    sceVideoOutSetBufferAttribute(&attribute, 0x80000000u, 1, 0, 1920, 1080, 1920);
    Require(attribute.pixel_format == 0x80000000u);
    Require(attribute.tiling_mode == 1);
    Require(attribute.aspect_ratio == 0);
    Require(attribute.width == 1920);
    Require(attribute.height == 1080);
    Require(attribute.pitch_in_pixel == 1920);
    Require(attribute.option == 0);
    Require(attribute.reserved0 == 0);
    Require(attribute.reserved1 == 0);

    ExpectFailure([] {
        sceVideoOutSetBufferAttribute2(nullptr, 0x8000000000000000ull, 1, 1920, 1080, 0, 0, 0);
    });

    VideoOutBufferAttribute2 attribute2{};
    sceVideoOutSetBufferAttribute2(&attribute2, 0x8000000000000000ull, 1, 1920, 1080, 0, 0, 0);
    Require(attribute2.pixel_format == 0x8000000000000000ull);
    Require(attribute2.tiling_mode == 1);
    Require(attribute2.width == 1920);
    Require(attribute2.height == 1080);
    Require(attribute2.option == 0);
    Require(attribute2.dcc_control == 0);
    Require(attribute2.dcc_cb_register_clear_color == 0);

    ExpectFailure([] {
        sceVideoOutAddBufferAttributeOption(nullptr, 1);
    });
    ExpectFailure([] {
        sceVideoOutRemoveBufferAttributeOption(nullptr, 1);
    });

    void* bufferAddress = nullptr;
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(-1, 0, &bufferAddress, 1, &attribute);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(0, 0, &bufferAddress, 1, &attribute);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(9999, 0, &bufferAddress, 1, &attribute);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(-1, 0, &bufferAddress, 1, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(-1, 0, nullptr, 1, &attribute);
    });

    ExpectFailure([&] {
        sceVideoOutSubmitChangeBufferAttribute2(-1, 0, &attribute2, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutSubmitChangeBufferAttribute2(0, 0, &attribute2, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutSubmitChangeBufferAttribute2(9999, 0, &attribute2, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutSubmitChangeBufferAttribute2(-1, 0, nullptr, nullptr);
    });

    std::array<VideoOutBuffers, 2> dummyBuffers{};
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers2(-1, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers2(0, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers2(9999, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers2(-1, 0, 0, dummyBuffers.data(), 1, nullptr, 0, nullptr);
    });

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error&) {
        LibcRunShutdown_nid_postfix();
        return 77;
    }
    Require(handle > 0);

    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(handle, 0, nullptr, 1, &attribute);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(handle, -1, &bufferAddress, 1, &attribute);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(handle, 16, &bufferAddress, 1, &attribute);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 0, &attribute);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 17, &attribute);
    });

    ExpectFailure([&] {
        sceVideoOutSubmitChangeBufferAttribute2(handle, 0, nullptr, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutSubmitChangeBufferAttribute2(handle, -1, &attribute2, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutSubmitChangeBufferAttribute2(handle, 16, &attribute2, nullptr);
    });

    ExpectFailure([&] {
        sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, nullptr, 0, nullptr);
    });
    Require(sceVideoOutRegisterBuffers2(handle, 0, 15, dummyBuffers.data(), 2, &attribute2, 0, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_VALUE);

    Require(sceVideoOutClose(handle) == 0);
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers(handle, 0, &bufferAddress, 1, &attribute);
    });
    ExpectFailure([&] {
        sceVideoOutSubmitChangeBufferAttribute2(handle, 0, &attribute2, nullptr);
    });
    ExpectFailure([&] {
        sceVideoOutRegisterBuffers2(handle, 0, 0, dummyBuffers.data(), 1, &attribute2, 0, nullptr);
    });

    LibcRunShutdown_nid_postfix();
    return 0;
}
