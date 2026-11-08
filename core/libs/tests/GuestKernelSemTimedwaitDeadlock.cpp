#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ETIMEDOUT = 60;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static KernelTimespec RealtimeAfter(std::int64_t nanoseconds) {
    KernelTimespec now{};
    Require(sceKernelClockGettime(0, &now) == 0);
    const std::int64_t totalNanos = now.tv_nsec + nanoseconds;
    const std::int64_t secAdjust = totalNanos / 1000000000LL;
    std::int64_t nsec = totalNanos % 1000000000LL;
    std::int64_t sec = now.tv_sec + secAdjust;
    if (nsec < 0) {
        nsec += 1000000000LL;
        sec -= 1;
    }
    return KernelTimespec{sec, nsec};
}

static void TestUninitializedSemaphore() {
    const KernelTimespec deadline = RealtimeAfter(1000000000LL);
    const KernelTimespec invalidTimespec{0, -1};

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(nullptr, &deadline) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(nullptr, &invalidTimespec) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    std::uintptr_t uninitSem = 0;
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&uninitSem, &deadline) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&uninitSem, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&uninitSem, &invalidTimespec) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    std::uintptr_t destroyedSem = 0;
    Require(sem_init_nid_postfix(&destroyedSem, 0, 1) == 0);
    Require(sem_destroy_nid_postfix(&destroyedSem) == 0);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&destroyedSem, &deadline) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&destroyedSem, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&destroyedSem, &invalidTimespec) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);
}

static void TestNegativeTimespec() {
    std::uintptr_t sem = 0;
    Require(sem_init_nid_postfix(&sem, 0, 0) == 0);

    KernelTimespec now{};
    Require(sceKernelClockGettime(0, &now) == 0);

    const KernelTimespec negativeNsecs[] = {
        {now.tv_sec + 1, -1},
        {now.tv_sec + 1, -1000000000LL},
        {0, -1},
        {-1, -1},
        {-10, -500}
    };
    for (const auto& ts : negativeNsecs) {
        *__error_nid_postfix() = 0;
        Require(sem_timedwait_nid_postfix(&sem, &ts) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
    }

    const KernelTimespec overflowNsecs[] = {
        {now.tv_sec + 1, 1000000000LL},
        {now.tv_sec + 1, 2000000000LL},
        {0, 1000000000LL},
        {-1, 1000000000LL}
    };
    for (const auto& ts : overflowNsecs) {
        *__error_nid_postfix() = 0;
        Require(sem_timedwait_nid_postfix(&sem, &ts) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
    }

    const KernelTimespec negativeSecs[] = {
        {-1, 0},
        {-10, 500000000LL},
        {-1000, 999999999LL}
    };
    for (const auto& ts : negativeSecs) {
        *__error_nid_postfix() = 0;
        Require(sem_timedwait_nid_postfix(&sem, &ts) == -1);
        Require(*__error_nid_postfix() == GUEST_ETIMEDOUT);
    }

    int value = -1;
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0);

    const KernelTimespec invalidNsec{now.tv_sec + 1, -1};
    Require(sem_post_nid_postfix(&sem) == 0);
    Require(sem_timedwait_nid_postfix(&sem, &invalidNsec) == 0);
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0);

    const KernelTimespec overflowNsec{now.tv_sec + 1, 1000000000LL};
    Require(sem_post_nid_postfix(&sem) == 0);
    Require(sem_timedwait_nid_postfix(&sem, &overflowNsec) == 0);
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0);

    const KernelTimespec negSec{-1, 0};
    Require(sem_post_nid_postfix(&sem) == 0);
    Require(sem_timedwait_nid_postfix(&sem, &negSec) == 0);
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0);

    Require(sem_destroy_nid_postfix(&sem) == 0);
}

static void TestDeadlineExpirationAndDeadlockAvoidance() {
    std::uintptr_t sem = 0;
    Require(sem_init_nid_postfix(&sem, 0, 0) == 0);

    KernelTimespec now{};
    Require(sceKernelClockGettime(0, &now) == 0);

    const KernelTimespec past{now.tv_sec > 5 ? now.tv_sec - 5 : 0, 0};
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&sem, &past) == -1);
    Require(*__error_nid_postfix() == GUEST_ETIMEDOUT);

    const KernelTimespec zeroTime{0, 0};
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&sem, &zeroTime) == -1);
    Require(*__error_nid_postfix() == GUEST_ETIMEDOUT);

    int value = -1;
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0);

    const auto deadline = RealtimeAfter(50000000LL);
    const auto start = std::chrono::steady_clock::now();
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&sem, &deadline) == -1);
    Require(*__error_nid_postfix() == GUEST_ETIMEDOUT);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(elapsed >= std::chrono::milliseconds(30));
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0);

    std::atomic<bool> threadStarted{false};
    std::atomic<int> waitResult{-999};
    const auto futureDeadline = RealtimeAfter(3000000000LL);
    std::thread waiter([&] {
        threadStarted.store(true, std::memory_order_release);
        waitResult.store(sem_timedwait_nid_postfix(&sem, &futureDeadline), std::memory_order_release);
    });
    while (!threadStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto postedTime = std::chrono::steady_clock::now();
    Require(sem_post_nid_postfix(&sem) == 0);
    waiter.join();
    Require(waitResult.load(std::memory_order_acquire) == 0);
    Require(std::chrono::steady_clock::now() - postedTime < std::chrono::seconds(2));
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0);

    Require(sem_destroy_nid_postfix(&sem) == 0);
}

int main() {
    TestUninitializedSemaphore();
    TestNegativeTimespec();
    TestDeadlineExpirationAndDeadlockAvoidance();
    return 0;
}
