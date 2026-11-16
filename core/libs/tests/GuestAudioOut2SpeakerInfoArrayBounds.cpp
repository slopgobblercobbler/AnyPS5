#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
constexpr int ErrorNotInitialized = static_cast<int>(0x80268006);
constexpr int ErrorInvalidPointer = static_cast<int>(0x8026800C);

struct GuestVbapParams {
    const AudioOut2Position* positions;
    std::uint32_t numSpeakers;
    std::uint8_t is3d;
    void* memory;
    std::size_t memorySize;
    std::uint32_t reserved;
};

struct AmbisonicsEncodeParam {
    std::uint32_t kind;
    std::int32_t order;
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Check failed at %s:%u\n", location.file_name(), location.line());
        std::abort();
    }
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void TestSpeakerInfoUninitialized() {
    AudioOut2SpeakerInfo info{};
    Require(sceAudioOut2GetSpeakerInfo(&info, 0) == ErrorNotInitialized);
    Require(sceAudioOut2GetSpeakerInfo(nullptr, 0) == ErrorNotInitialized);
    Require(sceAudioOut2GetSpeakerInfo(&info, 1) == ErrorNotInitialized);
    Require(sceAudioOut2GetSpeakerInfo(&info, 99) == ErrorNotInitialized);

    Require(sceAudioOut2Initialize() == 0 || sceAudioOut2Initialize() == static_cast<int>(0x80268004));
}

void TestSpeakerInfoValidation() {
    AudioOut2SpeakerInfo info{};
    Require(sceAudioOut2GetSpeakerInfo(nullptr, 0) == ErrorInvalidPointer);
    Require(sceAudioOut2GetSpeakerInfo(nullptr, 1) == ErrorInvalidPointer);

    for (const std::uint32_t badFlag : {2u, 3u, 10u, 100u, 0xFFFFFFFFu}) {
        Require(sceAudioOut2GetSpeakerInfo(&info, badFlag) == ErrorInvalidParam);
    }

    Require(sceAudioOut2GetSpeakerInfo(&info, 0) == 0);
    Require(info.type == 0);
    Require(info.available_bits == 0x3);
    Require(info.speaker_angle[0].azimuth == 30 && info.speaker_angle[0].elevation == 0);
    Require(info.speaker_angle[1].azimuth == -30 && info.speaker_angle[1].elevation == 0);
    for (std::size_t i = 2; i < 16; ++i) {
        Require(info.speaker_angle[i].azimuth == -360 && info.speaker_angle[i].elevation == -360);
    }

    AudioOut2SpeakerInfo info1{};
    Require(sceAudioOut2GetSpeakerInfo(&info1, 1) == 0);
    Require(info1.type == 0);
    Require(info1.available_bits == 0x3);
    Require(info1.speaker_angle[0].azimuth == 30 && info1.speaker_angle[0].elevation == 0);
    Require(info1.speaker_angle[1].azimuth == -30 && info1.speaker_angle[1].elevation == 0);
    for (std::size_t i = 2; i < 16; ++i) {
        Require(info1.speaker_angle[i].azimuth == -360 && info1.speaker_angle[i].elevation == -360);
    }
}

void TestSpeakerArrayMemorySize() {
    Require(Throws([] { sceAudioOut2GetSpeakerArrayMemorySize(0, 0, 0); }));
    Require(Throws([] { sceAudioOut2GetSpeakerArrayMemorySize(33, 0, 0); }));
    Require(Throws([] { sceAudioOut2GetSpeakerArrayMemorySize(100, 1, 1); }));

    for (const std::uint32_t count : {1u, 2u, 4u, 8u, 16u, 32u}) {
        const std::size_t sz1 = sceAudioOut2GetSpeakerArrayMemorySize(count, 0, 0);
        const std::size_t sz2 = sceAudioOut2GetSpeakerArrayMemorySize(count, 1, 1);
        Require(sz1 > 0);
        Require(sz1 == sz2);
    }
}

void TestSpeakerArrayLifecycle() {
    const std::size_t memSize = sceAudioOut2GetSpeakerArrayMemorySize(2, 0, 0);
    Require(memSize > 0);

    alignas(16) std::vector<std::uint8_t> buffer(memSize);
    std::array<AudioOut2Position, 2> positions{};
    positions[0] = {1.0f, 0.0f, 0.0f};
    positions[1] = {-1.0f, 0.0f, 0.0f};

    GuestVbapParams validParams{};
    validParams.positions = positions.data();
    validParams.numSpeakers = 2;
    validParams.is3d = 0;
    validParams.memory = buffer.data();
    validParams.memorySize = buffer.size();

    AudioOut2SpeakerArrayHandle handle = nullptr;

    Require(Throws([&] { sceAudioOut2SpeakerArrayCreate(nullptr, &validParams, nullptr); }));
    Require(Throws([&] { sceAudioOut2SpeakerArrayCreate(&handle, nullptr, nullptr); }));

    GuestVbapParams invalidParams = validParams;
    invalidParams.positions = nullptr;
    Require(Throws([&] { sceAudioOut2SpeakerArrayCreate(&handle, &invalidParams, nullptr); }));

    invalidParams = validParams;
    invalidParams.numSpeakers = 0;
    Require(Throws([&] { sceAudioOut2SpeakerArrayCreate(&handle, &invalidParams, nullptr); }));

    invalidParams = validParams;
    invalidParams.numSpeakers = 33;
    Require(Throws([&] { sceAudioOut2SpeakerArrayCreate(&handle, &invalidParams, nullptr); }));

    invalidParams = validParams;
    invalidParams.memory = nullptr;
    Require(Throws([&] { sceAudioOut2SpeakerArrayCreate(&handle, &invalidParams, nullptr); }));

    invalidParams = validParams;
    invalidParams.memorySize = sizeof(std::uint32_t);
    Require(Throws([&] { sceAudioOut2SpeakerArrayCreate(&handle, &invalidParams, nullptr); }));

    Require(sceAudioOut2SpeakerArrayCreate(&handle, &validParams, nullptr) == 0);
    Require(handle != nullptr);

    AudioOut2Position probePos{0.0f, 1.0f, 0.0f};
    std::array<float, 2> coeffs{};
    Require(Throws([&] { sceAudioOut2GetSpeakerArrayCoefficients(handle, probePos, 0.0f, nullptr, 2, 0, 0.0f); }));
    Require(Throws([&] { sceAudioOut2GetSpeakerArrayCoefficients(handle, probePos, 0.0f, coeffs.data(), 1, 0, 0.0f); }));
    Require(Throws([&] { sceAudioOut2GetSpeakerArrayCoefficients(handle, probePos, 0.0f, coeffs.data(), 3, 0, 0.0f); }));

    Require(sceAudioOut2GetSpeakerArrayCoefficients(handle, probePos, 0.0f, coeffs.data(), 2, 0, 0.0f) == 0);
    Require(coeffs[0] == 1.0f);
    Require(coeffs[1] == 1.0f);

    std::array<float, 2> ambiCoeffs{};
    Require(Throws([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(handle, 0x40, nullptr, 2); }));
    Require(Throws([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(handle, 0x40, ambiCoeffs.data(), 1); }));
    Require(Throws([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(handle, 0, ambiCoeffs.data(), 2); }));
    Require(Throws([&] { sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(handle, 0x40 | 36, ambiCoeffs.data(), 2); }));
    Require(sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(handle, 0x40, ambiCoeffs.data(), 2) == 0);

    Require(sceAudioOut2SpeakerArrayDestroy(handle) == 0);
    Require(Throws([&] { sceAudioOut2SpeakerArrayDestroy(handle); }));
    Require(Throws([&] { sceAudioOut2SpeakerArrayDestroy(nullptr); }));
    Require(Throws([&] { sceAudioOut2GetSpeakerArrayCoefficients(handle, probePos, 0.0f, coeffs.data(), 2, 0, 0.0f); }));
}

void TestAmbisonicsEncodeCoefficients() {
    AudioOut2Position dir{0.0f, 0.0f, 0.0f};
    std::array<float, 36> coeffs{};

    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(nullptr, coeffs.data(), dir) == ErrorInvalidParam);

    AmbisonicsEncodeParam param{};
    param.kind = 0;
    param.order = 0;
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, nullptr, dir) == ErrorInvalidParam);

    param.kind = 2;
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), dir) == ErrorInvalidParam);
    param.kind = 99;
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), dir) == ErrorInvalidParam);

    param.kind = 1;
    param.order = 4;
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), dir) == ErrorInvalidParam);
    param.order = -1;
    Require(Throws([&] { sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), dir); }));

    param.kind = 0;
    param.order = -1;
    Require(Throws([&] { sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), dir); }));
    param.order = 6;
    Require(Throws([&] { sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), dir); }));

    AudioOut2Position nonZeroDir{1.0f, 0.0f, 0.0f};
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), nonZeroDir) == 0);

    param.kind = 0;
    param.order = 0;
    coeffs.fill(0.0f);
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), dir) == 0);
    Require(coeffs[0] == 1.0f);

    param.kind = 1;
    param.order = 0;
    coeffs.fill(0.0f);
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), dir) == 0);
    const float expectedWeight = static_cast<float>(1.0 / std::sqrt(2.0));
    Require(std::abs(coeffs[0] - expectedWeight) < 1e-4f);

    param.kind = 0;
    param.order = 1;
    coeffs.fill(0.0f);
    Require(sceAudioOut2GetAmbisonicsEncodeCoefficients(&param, coeffs.data(), nonZeroDir) == 0);
    Require(coeffs[0] != 0.0f || coeffs[1] != 0.0f || coeffs[2] != 0.0f || coeffs[3] != 0.0f);
}

} // namespace

int main() {
    TestSpeakerInfoUninitialized();
    TestSpeakerInfoValidation();
    TestSpeakerArrayMemorySize();
    TestSpeakerArrayLifecycle();
    TestAmbisonicsEncodeCoefficients();

    LibcRunShutdown_nid_postfix();
    return 0;
}
