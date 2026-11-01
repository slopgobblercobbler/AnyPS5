#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    constexpr int noHandle = static_cast<int>(0x80920008);
    constexpr int validUserId = 0x10000000;
    constexpr int validUserId2 = 0x10000001;

    PadControllerInformation info{};

    for (int userId : {-9999, -100, -2, -1, 0, 1, 0x0FFFFFFF, 0x10000004, 0x20000000, 0x7FFFFFFF}) {
        Require(scePadGetHandle(userId, PAD_PORT_TYPE_STANDARD, 0) == noHandle);
        Require(scePadGetHandle(userId, PAD_PORT_TYPE_SPECIAL, 0) == noHandle);
    }

    for (int handle : {-9999, -100, -2, -1, 0, 5, 99, 1000}) {
        Require(scePadGetControllerInformation(handle, &info) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    Require(scePadGetHandle(validUserId, PAD_PORT_TYPE_STANDARD, 0) == noHandle);

    for (int userId : {-9999, -100, -2, -1, 0, 1, 0x0FFFFFFF, 0x10000004, 0x20000000, 0x7FFFFFFF}) {
        Require(scePadGetHandle(userId, PAD_PORT_TYPE_STANDARD, 0) == noHandle);
        Require(scePadGetHandle(userId, PAD_PORT_TYPE_SPECIAL, 0) == noHandle);
    }

    for (int handle : {-9999, -100, -2, -1, 0, 5, 99, 1000}) {
        Require(scePadGetControllerInformation(handle, &info) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(validUserId, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadGetHandle(validUserId, PAD_PORT_TYPE_STANDARD, 0) == handle);
    Require(scePadGetHandle(validUserId2, PAD_PORT_TYPE_STANDARD, 0) == noHandle);

    for (int userId : {-9999, -100, -2, -1, 0, 1, 0x0FFFFFFF, 0x10000004, 0x20000000, 0x7FFFFFFF}) {
        Require(scePadGetHandle(userId, PAD_PORT_TYPE_STANDARD, 0) == noHandle);
    }

    for (int type : {-1, 1, 3, 4, 15, 17, 100}) {
        Require(scePadGetHandle(validUserId, type, 0) == noHandle);
    }

    for (int idx : {-1, 1, 2, 100}) {
        Require(scePadGetHandle(validUserId, PAD_PORT_TYPE_STANDARD, idx) == noHandle);
    }

    Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);

    for (int invalidHandle : {-9999, -100, -2, -1, 0, handle + 99, 1000}) {
        Require(scePadGetControllerInformation(invalidHandle, &info) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    Require(scePadGetHandle(validUserId, PAD_PORT_TYPE_STANDARD, 0) == noHandle);
    Require(scePadGetControllerInformation(handle, &info) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadGetHandle(validUserId, PAD_PORT_TYPE_SPECIAL, 0) == noHandle);

    const int special = scePadOpen_nid_postfix(validUserId, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    Require(scePadGetHandle(validUserId, PAD_PORT_TYPE_SPECIAL, 0) == special);

    Require(scePadGetControllerInformation(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerInformation(special, &info) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);

    Require(scePadGetHandle(validUserId, PAD_PORT_TYPE_SPECIAL, 0) == noHandle);
    Require(scePadGetControllerInformation(special, &info) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetControllerInformation(special, nullptr) == PAD_ERROR_INVALID_HANDLE);

    return 0;
}
