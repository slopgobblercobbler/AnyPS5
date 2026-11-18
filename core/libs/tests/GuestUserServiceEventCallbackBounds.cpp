#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Predicate>
void WaitUntil(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate()) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

struct CallbackContext {
    std::atomic<int> callCount{};
    std::atomic<std::uint32_t> lastEventType{0xFFFFFFFFu};
    std::atomic<int> lastUserId{-1};
    std::atomic<void*> lastUserData{nullptr};
};

void APS5_VABI CallbackA(const SceUserServiceEvent* event, void* userData) {
    if (event != nullptr && userData != nullptr) {
        auto* ctx = static_cast<CallbackContext*>(userData);
        ctx->lastEventType.store(event->event_type);
        ctx->lastUserId.store(event->user_id);
        ctx->lastUserData.store(userData);
        ctx->callCount.fetch_add(1);
    }
}

void APS5_VABI CallbackB(const SceUserServiceEvent* event, void* userData) {
    if (event != nullptr && userData != nullptr) {
        auto* ctx = static_cast<CallbackContext*>(userData);
        ctx->lastEventType.store(event->event_type);
        ctx->lastUserId.store(event->user_id);
        ctx->lastUserData.store(userData);
        ctx->callCount.fetch_add(1);
    }
}

void APS5_VABI CallbackC(const SceUserServiceEvent* event, void* userData) {
    if (event != nullptr && userData != nullptr) {
        auto* ctx = static_cast<CallbackContext*>(userData);
        ctx->lastEventType.store(event->event_type);
        ctx->lastUserId.store(event->user_id);
        ctx->lastUserData.store(userData);
        ctx->callCount.fetch_add(1);
    }
}

void DrainEventQueue() {
    SceUserServiceEvent ev{};
    while (sceUserServiceGetEvent(&ev) == USER_SERVICE_OK) {}
}

void TestPreInitializationBounds() {
    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(CallbackA, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(CallbackA, reinterpret_cast<void*>(0x1234)) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(CallbackB, reinterpret_cast<void*>(0x5678)) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(CallbackA, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(CallbackA, reinterpret_cast<void*>(0x1234)) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(CallbackB, reinterpret_cast<void*>(0x5678)) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestNullAndUnregisteredArgumentBounds() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    DrainEventQueue();

    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(nullptr, reinterpret_cast<void*>(0xABCD)) == USER_SERVICE_OK);

    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceUnregisterEventCallback(nullptr, reinterpret_cast<void*>(0xABCD)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceUnregisterEventCallback(CallbackA, nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(CallbackA, reinterpret_cast<void*>(0x1234)) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(CallbackB, reinterpret_cast<void*>(0x5678)) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(CallbackC, reinterpret_cast<void*>(0x9ABC)) == USER_SERVICE_OK);
}

void TestDuplicateAndDistinctRegistrations() {
    CallbackContext ctxA1;
    CallbackContext ctxA2;
    CallbackContext ctxB1;

    Require(sceUserServiceRegisterEventCallback(CallbackA, &ctxA1) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CallbackA, &ctxA1) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CallbackA, &ctxA2) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CallbackB, &ctxA1) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CallbackB, &ctxB1) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(1, true);

    WaitUntil([&] {
        return ctxA1.callCount.load() == 2 &&
               ctxA2.callCount.load() == 1 &&
               ctxB1.callCount.load() == 1;
    });

    Require(ctxA1.lastEventType.load() == 0u);
    Require(ctxA1.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(ctxA1.lastUserData.load() == &ctxA1);

    Require(ctxA2.lastEventType.load() == 0u);
    Require(ctxA2.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(ctxA2.lastUserData.load() == &ctxA2);

    Require(ctxB1.lastEventType.load() == 0u);
    Require(ctxB1.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(ctxB1.lastUserData.load() == &ctxB1);

    UserServiceSetUserConnected_nid_postfix(1, false);

    WaitUntil([&] {
        return ctxA1.callCount.load() == 4 &&
               ctxA2.callCount.load() == 2 &&
               ctxB1.callCount.load() == 2;
    });

    Require(ctxA1.lastEventType.load() == 1u);
    Require(ctxA1.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(ctxA2.lastEventType.load() == 1u);
    Require(ctxA2.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(ctxB1.lastEventType.load() == 1u);
    Require(ctxB1.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);

    Require(sceUserServiceUnregisterEventCallback(CallbackA, &ctxA1) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(CallbackA, &ctxA2) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(CallbackB, &ctxA1) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(CallbackB, &ctxB1) == USER_SERVICE_OK);

    DrainEventQueue();
}

void TestSelectiveDeregistrationAndReRegistration() {
    CallbackContext ctx1;
    CallbackContext ctx2;

    Require(sceUserServiceRegisterEventCallback(CallbackA, &ctx1) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CallbackB, &ctx2) == USER_SERVICE_OK);

    Require(sceUserServiceUnregisterEventCallback(CallbackA, &ctx2) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(2, true);

    WaitUntil([&] {
        return ctx1.callCount.load() == 1 && ctx2.callCount.load() == 1;
    });

    Require(ctx1.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 2);
    Require(ctx2.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 2);

    Require(sceUserServiceUnregisterEventCallback(CallbackA, &ctx1) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(2, false);

    WaitUntil([&] {
        return ctx2.callCount.load() == 2;
    });

    Require(ctx1.callCount.load() == 1);
    Require(ctx2.lastEventType.load() == 1u);

    Require(sceUserServiceRegisterEventCallback(CallbackA, &ctx1) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(3, true);

    WaitUntil([&] {
        return ctx1.callCount.load() == 2 && ctx2.callCount.load() == 3;
    });

    Require(ctx1.lastEventType.load() == 0u);
    Require(ctx1.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 3);
    Require(ctx2.lastEventType.load() == 0u);
    Require(ctx2.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 3);

    UserServiceSetUserConnected_nid_postfix(3, false);

    WaitUntil([&] {
        return ctx1.callCount.load() == 3 && ctx2.callCount.load() == 4;
    });

    Require(sceUserServiceUnregisterEventCallback(CallbackA, &ctx1) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(CallbackB, &ctx2) == USER_SERVICE_OK);

    DrainEventQueue();
}

void TestManyCallbacksStressBounds() {
    constexpr std::size_t kContextCount = 12;
    std::array<CallbackContext, kContextCount> contexts;

    for (std::size_t i = 0; i < kContextCount; ++i) {
        UserServiceEventCallback fn = (i % 3 == 0) ? CallbackA : ((i % 3 == 1) ? CallbackB : CallbackC);
        Require(sceUserServiceRegisterEventCallback(fn, &contexts[i]) == USER_SERVICE_OK);
    }

    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(2, true);

    for (std::size_t i = 0; i < kContextCount; ++i) {
        WaitUntil([&contexts, i] {
            return contexts[i].callCount.load() >= 2;
        });
        Require(contexts[i].callCount.load() == 2);
        Require(contexts[i].lastEventType.load() == 0u);
        Require(contexts[i].lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 2);
    }

    UserServiceSetUserConnected_nid_postfix(1, false);
    UserServiceSetUserConnected_nid_postfix(2, false);

    for (std::size_t i = 0; i < kContextCount; ++i) {
        WaitUntil([&contexts, i] {
            return contexts[i].callCount.load() >= 4;
        });
        Require(contexts[i].callCount.load() == 4);
        Require(contexts[i].lastEventType.load() == 1u);
        Require(contexts[i].lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 2);
    }

    for (std::size_t i = 0; i < kContextCount; ++i) {
        UserServiceEventCallback fn = (i % 3 == 0) ? CallbackA : ((i % 3 == 1) ? CallbackB : CallbackC);
        Require(sceUserServiceUnregisterEventCallback(fn, &contexts[i]) == USER_SERVICE_OK);
        Require(sceUserServiceUnregisterEventCallback(fn, &contexts[i]) == USER_SERVICE_OK);
    }

    UserServiceSetUserConnected_nid_postfix(3, true);
    UserServiceSetUserConnected_nid_postfix(3, false);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (std::size_t i = 0; i < kContextCount; ++i) {
        Require(contexts[i].callCount.load() == 4);
    }

    DrainEventQueue();
}

void TestTerminationPurgesCallbacks() {
    CallbackContext ctxA;
    CallbackContext ctxB;

    Require(sceUserServiceRegisterEventCallback(CallbackA, &ctxA) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CallbackB, &ctxB) == USER_SERVICE_OK);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceRegisterEventCallback(CallbackA, &ctxA) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(CallbackA, &ctxA) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    DrainEventQueue();

    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(1, false);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Require(ctxA.callCount.load() == 0);
    Require(ctxB.callCount.load() == 0);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestNullAndUnregisteredArgumentBounds();
    TestDuplicateAndDistinctRegistrations();
    TestSelectiveDeregistrationAndReRegistration();
    TestManyCallbacksStressBounds();
    TestTerminationPurgesCallbacks();
    return 0;
}
