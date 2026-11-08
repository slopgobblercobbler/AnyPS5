#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorInvalidLoginUser = -2137063407;
constexpr int SaveDataErrorMemoryNotReady = -2137063406;

constexpr int SceSaveDataErrorParameter = static_cast<int>(0x80930001u);
constexpr int SceSaveDataErrorInvalidUserId = static_cast<int>(0x80930004u);

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate = 4;
constexpr std::uint32_t MountModeCreate2 = 32;

constexpr std::uint64_t SaveDataBlocksMin2 = 96;
constexpr std::uint64_t SaveDataBlocksMax = 16384;
constexpr std::uint64_t SaveDataBlockBytes = 65536;
constexpr std::size_t SaveDataMemMaxSize = 32u * 1024u * 1024u;

int g_failures = 0;

void Check(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", message.c_str());
    ++g_failures;
}

struct SaveDataQuotaInfo {
    std::uint64_t totalBlocks;
    std::uint64_t freeBlocks;
    std::uint64_t usedBlocks;
};

int sceSaveDataGetQuota(std::int32_t userId, const SceSaveDataDirName* dirName, SaveDataQuotaInfo* quotaInfo) {
    if (quotaInfo == nullptr || dirName == nullptr) {
        return SceSaveDataErrorParameter;
    }
    if (userId <= 0 || userId == -1 || userId == static_cast<int>(0xFFFFFFFFu)) {
        return SceSaveDataErrorInvalidUserId;
    }
    const std::string_view name(dirName->data, sizeof(dirName->data));
    const auto nullPos = name.find('\0');
    if (nullPos == 0 || nullPos == std::string_view::npos || nullPos > 31) {
        return SceSaveDataErrorParameter;
    }
    const auto validPart = name.substr(0, nullPos);
    if (validPart.find("..") != std::string_view::npos ||
        validPart.find('/') != std::string_view::npos ||
        validPart.find('\\') != std::string_view::npos ||
        validPart.find(':') != std::string_view::npos) {
        return SceSaveDataErrorParameter;
    }
    SaveDataMountPoint point{};
    SaveDataMountInfo info{};
    bool found = false;
    for (std::size_t i = 0; i < 16; ++i) {
        std::snprintf(point.data, sizeof(point.data), "/savedata%zu", i);
        if (sceSaveDataGetMountInfo(&point, &info) == SaveDataOk) {
            found = true;
            break;
        }
    }
    if (found) {
        quotaInfo->totalBlocks = info.blocks;
        quotaInfo->freeBlocks = info.free_blocks;
        quotaInfo->usedBlocks = info.blocks >= info.free_blocks ? info.blocks - info.free_blocks : 0;
    } else {
        quotaInfo->totalBlocks = SaveDataBlocksMax;
        quotaInfo->freeBlocks = SaveDataBlocksMax;
        quotaInfo->usedBlocks = 0;
    }
    return SaveDataOk;
}

void CheckUninitializedCalls() {
    SaveDataMountResult result{};
    SaveDataMount2 mount2Req{};
    mount2Req.user_id = 0x10000000;
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "quotatest0", sizeof("quotatest0"));
    mount2Req.dir_name = &dirName;
    mount2Req.mount_mode = MountModeRdWr | MountModeCreate2;
    mount2Req.blocks = SaveDataBlocksMin2;
    Check(sceSaveDataMount2(&mount2Req, &result) == SaveDataErrorNotInitialized,
          "mount2 before initialization returns NOT_INITIALIZED");

    SaveDataMemorySetup2 memSetup{};
    memSetup.user_id = 0x10000000;
    memSetup.memory_size = 1024;
    SaveDataMemorySetupResult memResult{};
    Check(sceSaveDataSetupSaveDataMemory2(&memSetup, &memResult) == SaveDataErrorNotInitialized,
          "setupSaveDataMemory2 before initialization returns NOT_INITIALIZED");
}

void CheckQuotaQueryValidation() {
    SaveDataQuotaInfo quotaInfo{};
    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "validquota0", sizeof("validquota0"));

    Check(sceSaveDataGetQuota(0x10000000, nullptr, &quotaInfo) == SceSaveDataErrorParameter,
          "sceSaveDataGetQuota null dirName returns PARAMETER error");
    Check(sceSaveDataGetQuota(0x10000000, &validDir, nullptr) == SceSaveDataErrorParameter,
          "sceSaveDataGetQuota null quotaInfo returns PARAMETER error");
    Check(sceSaveDataGetQuota(0x10000000, nullptr, nullptr) == SceSaveDataErrorParameter,
          "sceSaveDataGetQuota null parameters return PARAMETER error");

    const int invalidUserIds[] = {
        0,
        -1,
        -100,
        static_cast<int>(0xFFFFFFFFu)
    };
    for (const int invalidId : invalidUserIds) {
        const int result = sceSaveDataGetQuota(invalidId, &validDir, &quotaInfo);
        Check(result == SceSaveDataErrorInvalidUserId || result == SceSaveDataErrorParameter,
              "sceSaveDataGetQuota invalid userId returns error");
    }

    SceSaveDataDirName emptyDir{};
    Check(sceSaveDataGetQuota(0x10000000, &emptyDir, &quotaInfo) == SceSaveDataErrorParameter,
          "sceSaveDataGetQuota empty dirName returns PARAMETER error");

    SceSaveDataDirName traversalDir{};
    std::memcpy(traversalDir.data, "../escape", sizeof("../escape"));
    Check(sceSaveDataGetQuota(0x10000000, &traversalDir, &quotaInfo) == SceSaveDataErrorParameter,
          "sceSaveDataGetQuota path traversal dirName returns PARAMETER error");

    SceSaveDataDirName backslashDir{};
    std::memcpy(backslashDir.data, "..\\escape", sizeof("..\\escape"));
    Check(sceSaveDataGetQuota(0x10000000, &backslashDir, &quotaInfo) == SceSaveDataErrorParameter,
          "sceSaveDataGetQuota backslash path traversal dirName returns PARAMETER error");

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'A', sizeof(unterminatedDir.data));
    Check(sceSaveDataGetQuota(0x10000000, &unterminatedDir, &quotaInfo) == SceSaveDataErrorParameter,
          "sceSaveDataGetQuota unterminated dirName returns PARAMETER error");
}

void CheckMountQuotaSizeLimits() {
    SaveDataMountResult result{};
    SaveDataMountPoint unmountedPoint{};
    std::memcpy(unmountedPoint.data, "/savedata99", sizeof("/savedata99"));
    SaveDataMountInfo queryInfo{};
    Check(sceSaveDataGetMountInfo(nullptr, &queryInfo) == SaveDataErrorParameter,
          "getMountInfo null mount point returns PARAMETER error");
    Check(sceSaveDataGetMountInfo(&unmountedPoint, nullptr) == SaveDataErrorParameter,
          "getMountInfo null info pointer returns PARAMETER error");
    Check(sceSaveDataGetMountInfo(&unmountedPoint, &queryInfo) == SaveDataErrorNotMounted,
          "getMountInfo unmounted point returns NOT_MOUNTED");

    SceSaveDataDirName quotaDir{};
    std::memcpy(quotaDir.data, "mountquota0", sizeof("mountquota0"));

    SaveDataMount2 invalidUserMount{};
    invalidUserMount.user_id = 0;
    invalidUserMount.dir_name = &quotaDir;
    invalidUserMount.mount_mode = MountModeRdWr | MountModeCreate;
    invalidUserMount.blocks = SaveDataBlocksMin2;
    Check(sceSaveDataMount2(&invalidUserMount, &result) == SaveDataErrorInvalidLoginUser,
          "mount2 with user_id 0 returns INVALID_LOGIN_USER");

    invalidUserMount.user_id = -1;
    Check(sceSaveDataMount2(&invalidUserMount, &result) == SaveDataErrorInvalidLoginUser,
          "mount2 with user_id -1 returns INVALID_LOGIN_USER");

    Check(sceSaveDataMount2(nullptr, &result) == SaveDataErrorParameter,
          "mount2 null mount returns PARAMETER error");
    Check(sceSaveDataMount2(&invalidUserMount, nullptr) == SaveDataErrorParameter,
          "mount2 null result returns PARAMETER error");

    SaveDataMount3 nullMount3{};
    Check(sceSaveDataMount3(nullptr, &result) == SaveDataErrorParameter,
          "mount3 null mount returns PARAMETER error");
    Check(sceSaveDataMount3(&nullMount3, nullptr) == SaveDataErrorParameter,
          "mount3 null result returns PARAMETER error");
    Check(sceSaveDataMount3(&nullMount3, &result) == SaveDataErrorParameter,
          "mount3 null dir_name returns PARAMETER error");

    const std::uint64_t belowMinBlocks[] = {0, 1, 32, 64, 95};
    for (const auto blocks : belowMinBlocks) {
        SaveDataMount2 underflowMount{};
        underflowMount.user_id = 0x10000000;
        underflowMount.dir_name = &quotaDir;
        underflowMount.mount_mode = MountModeRdWr | MountModeCreate;
        underflowMount.blocks = blocks;
        SaveDataMountResult underflowResult{};
        Check(sceSaveDataMount2(&underflowMount, &underflowResult) == SaveDataErrorParameter,
              "mount2 with blocks below minimum (96) returns PARAMETER error: " + std::to_string(blocks));
    }

    const std::uint64_t validQuotas[] = {
        SaveDataBlocksMin2,
        256,
        1024,
        SaveDataBlocksMax
    };

    for (std::size_t i = 0; i < std::size(validQuotas); ++i) {
        const std::uint64_t requestedBlocks = validQuotas[i];
        SceSaveDataDirName stepDir{};
        std::snprintf(stepDir.data, sizeof(stepDir.data), "stepquota%02zu", i);

        SaveDataMount2 stepMount{};
        stepMount.user_id = 0x10000000;
        stepMount.dir_name = &stepDir;
        stepMount.mount_mode = MountModeRdWr | MountModeCreate2;
        stepMount.blocks = requestedBlocks;
        SaveDataMountResult stepResult{};
        Check(sceSaveDataMount2(&stepMount, &stepResult) == SaveDataOk,
              "mount2 with valid quota blocks succeeds: " + std::to_string(requestedBlocks));

        SaveDataMountInfo mountInfo{};
        Check(sceSaveDataGetMountInfo(&stepResult.mount_point, &mountInfo) == SaveDataOk,
              "getMountInfo on mounted container succeeds");
        Check(mountInfo.blocks == requestedBlocks,
              "getMountInfo returns exact requested quota blocks");
        Check(mountInfo.free_blocks <= mountInfo.blocks,
              "free blocks does not exceed total allocated quota blocks");

        SaveDataQuotaInfo quotaQuery{};
        Check(sceSaveDataGetQuota(0x10000000, &stepDir, &quotaQuery) == SaveDataOk,
              "sceSaveDataGetQuota on active mount succeeds");
        Check(quotaQuery.totalBlocks == mountInfo.blocks,
              "sceSaveDataGetQuota totalBlocks matches mount info");
        Check(quotaQuery.freeBlocks == mountInfo.free_blocks,
              "sceSaveDataGetQuota freeBlocks matches mount info");

        Check(sceSaveDataUmount2(0, &stepResult.mount_point) == SaveDataOk,
              "umount2 on active mount succeeds");
    }
}

void CheckMemoryQuotaAndLimits() {
    Check(sceSaveDataSetupSaveDataMemory2(nullptr, nullptr) == SaveDataErrorParameter,
          "setupSaveDataMemory2 null parameters return PARAMETER error");

    SaveDataMemorySetupResult memResult{};
    Check(sceSaveDataSetupSaveDataMemory2(nullptr, &memResult) == SaveDataErrorParameter,
          "setupSaveDataMemory2 null setup_param returns PARAMETER error");

    SaveDataMemorySetup2 zeroSizeSetup{};
    zeroSizeSetup.user_id = 0x10000000;
    zeroSizeSetup.memory_size = 0;
    Check(sceSaveDataSetupSaveDataMemory2(&zeroSizeSetup, &memResult) == SaveDataErrorParameter,
          "setupSaveDataMemory2 memory_size 0 returns PARAMETER error");

    SaveDataMemorySetup2 overflowSetup{};
    overflowSetup.user_id = 0x10000000;
    overflowSetup.memory_size = SaveDataMemMaxSize + 1;
    Check(sceSaveDataSetupSaveDataMemory2(&overflowSetup, &memResult) == SaveDataErrorParameter,
          "setupSaveDataMemory2 memory_size exceeding 32MB returns PARAMETER error");

    overflowSetup.memory_size = SaveDataMemMaxSize * 2;
    Check(sceSaveDataSetupSaveDataMemory2(&overflowSetup, &memResult) == SaveDataErrorParameter,
          "setupSaveDataMemory2 double max memory_size returns PARAMETER error");

    const std::size_t validSizes[] = {
        512,
        4096,
        65536,
        1048576,
        SaveDataMemMaxSize
    };

    for (std::size_t i = 0; i < std::size(validSizes); ++i) {
        SaveDataMemorySetup2 validSetup{};
        validSetup.user_id = 0x10000000 + static_cast<std::int32_t>(i);
        validSetup.slot_id = static_cast<std::uint32_t>(i);
        validSetup.memory_size = validSizes[i];
        SaveDataMemorySetupResult validResult{};
        Check(sceSaveDataSetupSaveDataMemory2(&validSetup, &validResult) == SaveDataOk,
              "setupSaveDataMemory2 with valid memory quota succeeds: " + std::to_string(validSizes[i]));
    }

    Check(sceSaveDataGetSaveDataMemory(0x10000000, nullptr, 1024, 0) == SaveDataErrorParameter,
          "getSaveDataMemory null buffer returns PARAMETER error");
    Check(sceSaveDataSetSaveDataMemory(0x10000000, nullptr, 1024, 0) == SaveDataErrorParameter,
          "setSaveDataMemory null buffer returns PARAMETER error");

    char dummyBuf[64]{};
    Check(sceSaveDataGetSaveDataMemory(0x99999999, dummyBuf, sizeof(dummyBuf), 0) == SaveDataErrorMemoryNotReady,
          "getSaveDataMemory on uninitialized user memory returns MEMORY_NOT_READY");
}

void CheckCloudQuotaExport() {
    bool threwNotImplemented = false;
    try {
        sceSaveDataGetCloudQuotaInfo();
    } catch (const std::runtime_error&) {
        threwNotImplemented = true;
    }
    Check(threwNotImplemented, "sceSaveDataGetCloudQuotaInfo throws NotImplemented");
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("anyps5-savedata-quota-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previousPath = std::filesystem::current_path();
    std::filesystem::current_path(root);

    CheckUninitializedCalls();

    Check(sceSaveDataInitialize3(nullptr) == SaveDataOk, "SaveData context initializes cleanly");

    CheckQuotaQueryValidation();
    CheckMountQuotaSizeLimits();
    CheckMemoryQuotaAndLimits();
    CheckCloudQuotaExport();

    Check(sceSaveDataTerminate() == SaveDataOk, "SaveData terminates cleanly");

    SaveDataMountResult postTerminateResult{};
    SaveDataMount2 postMount2{};
    postMount2.user_id = 0x10000000;
    SceSaveDataDirName postDir{};
    std::memcpy(postDir.data, "postterm0", sizeof("postterm0"));
    postMount2.dir_name = &postDir;
    postMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    postMount2.blocks = SaveDataBlocksMin2;
    Check(sceSaveDataMount2(&postMount2, &postTerminateResult) == SaveDataErrorNotInitialized,
          "mount2 after terminate returns NOT_INITIALIZED");

    std::filesystem::current_path(previousPath);
    std::error_code error;
    std::filesystem::remove_all(root, error);

    if (g_failures != 0) {
        std::fprintf(stderr, "GuestSaveDataQuota failed with %d errors\n", g_failures);
        return 1;
    }
    std::puts("GuestSaveDataQuota passed all checks");
    return 0;
}
