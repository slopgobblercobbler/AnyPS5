#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUnopenedAndInvalidHandleBounds() {
    PadVibrationParam param{128, 128};
    for (int handle : {-9999, -1, 0, 1, 0x10, 0x20, 9999}) {
        Require(scePadSetVibration(handle, &param) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestPreInitAndHandleOpen() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    PadVibrationParam param{100, 100};
    for (int handle : {-9999, -1, 0, 1, 0x10, 0x20, 9999}) {
        Require(scePadSetVibration(handle, &param) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableVibration(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestNullParamValidation(int handle) {
    Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_ARG);
}

void TestMotorIntensityBoundsAndOutputVerification(int handle) {
    std::uint32_t seq = 0;
    PadOutputState out{};

    PadVibrationParam param{0, 0};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 0);
    Require(out.vibrationSmall == 0);

    param = {255, 255};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 255);
    Require(out.vibrationSmall == 255);

    param = {200, 0};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 200);
    Require(out.vibrationSmall == 0);

    param = {0, 150};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 0);
    Require(out.vibrationSmall == 150);

    param = {64, 192};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 64);
    Require(out.vibrationSmall == 192);

    param = {1, 1};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 1);
    Require(out.vibrationSmall == 1);

    param = {127, 128};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 127);
    Require(out.vibrationSmall == 128);

    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(!PadFetchOutput_nid_postfix(&seq, &out));
}

void TestDisableVibrationBehavior(int handle) {
    std::uint32_t seq = 0;
    PadOutputState out{};

    PadVibrationParam param{180, 220};
    Require(scePadSetVibration(handle, &param) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 180);
    Require(out.vibrationSmall == 220);

    Require(scePadDisableVibration(handle, false) == PAD_OK);
    Require(!PadFetchOutput_nid_postfix(&seq, &out));

    Require(scePadDisableVibration(handle, true) == PAD_OK);
    Require(PadFetchOutput_nid_postfix(&seq, &out));
    Require(out.vibrationLarge == 0);
    Require(out.vibrationSmall == 0);
}

void TestMultiPortVibrationIsolation(int handle0) {
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0);

    PadVibrationParam param0{50, 75};
    PadVibrationParam param1{150, 225};
    Require(scePadSetVibration(handle0, &param0) == PAD_OK);
    Require(scePadSetVibration(handle1, &param1) == PAD_OK);

    std::uint32_t seq0 = 0;
    PadOutputState out0{};
    Require(PadFetchOutputForPort_nid_postfix(0, &seq0, &out0));
    Require(out0.vibrationLarge == 50);
    Require(out0.vibrationSmall == 75);

    std::uint32_t seq1 = 0;
    PadOutputState out1{};
    Require(PadFetchOutputForPort_nid_postfix(1, &seq1, &out1));
    Require(out1.vibrationLarge == 150);
    Require(out1.vibrationSmall == 225);

    Require(scePadDisableVibration(handle1, true) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(1, &seq1, &out1));
    Require(out1.vibrationLarge == 0);
    Require(out1.vibrationSmall == 0);
    Require(!PadFetchOutputForPort_nid_postfix(0, &seq0, &out0));

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
}

void TestSpecialPortHandleBehavior() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    PadVibrationParam param{100, 100};
    Require(scePadSetVibration(special, &param) == PAD_OK);
    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadDisableVibration(special, true) == PAD_OK);
    Require(scePadDisableVibration(special, false) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetVibration(special, &param) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableVibration(special, true) == PAD_ERROR_INVALID_HANDLE);
}

void TestHandleClosureAndCleanup(int handle) {
    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    PadVibrationParam param{10, 10};
    Require(scePadSetVibration(handle, &param) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableVibration(handle, true) == PAD_ERROR_INVALID_HANDLE);

    Pad::SetVibration(0, 0, 0);
    Pad::SetVibration(1, 0, 0);
}

} // namespace

int main() {
    TestUnopenedAndInvalidHandleBounds();
    TestPreInitAndHandleOpen();

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    TestNullParamValidation(handle);
    TestMotorIntensityBoundsAndOutputVerification(handle);
    TestDisableVibrationBehavior(handle);
    TestMultiPortVibrationIsolation(handle);
    TestSpecialPortHandleBehavior();
    TestHandleClosureAndCleanup(handle);

    return 0;
}
