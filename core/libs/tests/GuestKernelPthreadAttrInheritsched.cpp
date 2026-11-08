#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_ENOTSUP = 45;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EXPLICIT_SCHED = 0;
static constexpr int GUEST_INHERIT_SCHED = 4;
static constexpr int GUEST_SCHED_FIFO = 1;
static constexpr int GUEST_SCHED_OTHER = 2;
static constexpr int GUEST_SCHED_RR = 3;
static constexpr int GUEST_SCOPE_PROCESS = 0;
static constexpr int GUEST_SCOPE_SYSTEM = 2;

static void Require(bool value) {
    if (!value) std::abort();
}

class GuardPage {
public:
    GuardPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        memory = static_cast<char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
#endif
    }

    ~GuardPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * page);
#endif
    }

    GuardPage(const GuardPage&) = delete;
    GuardPage& operator=(const GuardPage&) = delete;

    void* Unreadable() const {
        return memory + page;
    }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

static void CheckInheritschedPointersAndFaults() {
    int inherit = -1;
    Require(pthread_attr_setinheritsched_nid_postfix(nullptr, GUEST_INHERIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(nullptr, GUEST_EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getinheritsched_nid_postfix(nullptr, &inherit) == GUEST_EINVAL && inherit == -1);

    Require(scePthreadAttrSetinheritsched(nullptr, GUEST_INHERIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetinheritsched(nullptr, GUEST_EXPLICIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetinheritsched(nullptr, &inherit) == SCE_KERNEL_ERROR_EINVAL && inherit == -1);

    PthreadAttr unset = nullptr;
    Require(pthread_attr_setinheritsched_nid_postfix(&unset, GUEST_INHERIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(&unset, GUEST_EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getinheritsched_nid_postfix(&unset, &inherit) == GUEST_EINVAL && inherit == -1);

    Require(scePthreadAttrSetinheritsched(&unset, GUEST_INHERIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetinheritsched(&unset, GUEST_EXPLICIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetinheritsched(&unset, &inherit) == SCE_KERNEL_ERROR_EINVAL && inherit == -1);

    PthreadAttr destroyed = nullptr;
    Require(scePthreadAttrInit(&destroyed) == SCE_OK);
    Require(scePthreadAttrDestroy(&destroyed) == SCE_OK);
    Require(pthread_attr_setinheritsched_nid_postfix(&destroyed, GUEST_INHERIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(&destroyed, GUEST_EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getinheritsched_nid_postfix(&destroyed, &inherit) == GUEST_EINVAL && inherit == -1);

    Require(scePthreadAttrSetinheritsched(&destroyed, GUEST_INHERIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetinheritsched(&destroyed, GUEST_EXPLICIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetinheritsched(&destroyed, &inherit) == SCE_KERNEL_ERROR_EINVAL && inherit == -1);

    PthreadAttr live = nullptr;
    Require(pthread_attr_init_nid_postfix(&live) == 0);
    Require(pthread_attr_getinheritsched_nid_postfix(&live, nullptr) == GUEST_EINVAL);
    Require(scePthreadAttrGetinheritsched(&live, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_attr_destroy_nid_postfix(&live) == 0);

    GuardPage guard;
    void* const badPointers[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    for (void* pointer : badPointers) {
        auto* badAttr = static_cast<PthreadAttr*>(pointer);
        auto* badInt = static_cast<int*>(pointer);
        Require(pthread_attr_setinheritsched_nid_postfix(badAttr, GUEST_INHERIT_SCHED) == GUEST_EFAULT);
        Require(pthread_attr_getinheritsched_nid_postfix(badAttr, &inherit) == GUEST_EFAULT && inherit == -1);
        Require(scePthreadAttrSetinheritsched(badAttr, GUEST_INHERIT_SCHED) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetinheritsched(badAttr, &inherit) == SCE_KERNEL_ERROR_EFAULT && inherit == -1);

        Require(pthread_attr_init_nid_postfix(&live) == 0);
        Require(pthread_attr_getinheritsched_nid_postfix(&live, badInt) == GUEST_EFAULT);
        Require(scePthreadAttrGetinheritsched(&live, badInt) == SCE_KERNEL_ERROR_EFAULT);
        Require(pthread_attr_destroy_nid_postfix(&live) == 0);
    }
}

static void CheckInheritschedDefaultsAndTransitions() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    int inherit = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == GUEST_INHERIT_SCHED);
    inherit = -1;
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == GUEST_INHERIT_SCHED);

    Require(pthread_attr_setinheritsched_nid_postfix(&attr, GUEST_EXPLICIT_SCHED) == 0);
    inherit = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == GUEST_EXPLICIT_SCHED);
    inherit = -1;
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == GUEST_EXPLICIT_SCHED);

    Require(pthread_attr_setinheritsched_nid_postfix(&attr, GUEST_INHERIT_SCHED) == 0);
    inherit = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == GUEST_INHERIT_SCHED);
    inherit = -1;
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == GUEST_INHERIT_SCHED);

    Require(scePthreadAttrSetinheritsched(&attr, GUEST_EXPLICIT_SCHED) == SCE_OK);
    inherit = -1;
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == GUEST_EXPLICIT_SCHED);
    inherit = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == GUEST_EXPLICIT_SCHED);

    Require(scePthreadAttrSetinheritsched(&attr, GUEST_INHERIT_SCHED) == SCE_OK);
    inherit = -1;
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == GUEST_INHERIT_SCHED);
    inherit = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == GUEST_INHERIT_SCHED);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
}

static void CheckInheritschedInvalidValuesRejection() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);

    const int invalidValues[] = {
        -2147483647 - 1, -1000, -2, -1, 1, 2, 3, 5, 6, 7, 8, 15, 16, 32, 64, 100, 255, 1000, 0x7fffffff
    };

    for (int initial : {GUEST_INHERIT_SCHED, GUEST_EXPLICIT_SCHED}) {
        Require(pthread_attr_setinheritsched_nid_postfix(&attr, initial) == 0);
        for (int invalid : invalidValues) {
            Require(pthread_attr_setinheritsched_nid_postfix(&attr, invalid) == GUEST_ENOTSUP);
            int inherit = -1;
            Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == initial);
            Require(scePthreadAttrSetinheritsched(&attr, invalid) == SCE_KERNEL_ERROR_EOPNOTSUPP);
            inherit = -1;
            Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == initial);
        }
    }

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
}

static void CheckSchedPolicyBoundsAndRejection() {
    int policy = -1;
    Require(pthread_attr_setschedpolicy_nid_postfix(nullptr, GUEST_SCHED_FIFO) == GUEST_EINVAL);
    Require(pthread_attr_getschedpolicy_nid_postfix(nullptr, &policy) == GUEST_EINVAL && policy == -1);

    Require(scePthreadAttrSetschedpolicy(nullptr, GUEST_SCHED_FIFO) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetschedpolicy(nullptr, &policy) == SCE_KERNEL_ERROR_EINVAL && policy == -1);

    PthreadAttr unset = nullptr;
    Require(pthread_attr_setschedpolicy_nid_postfix(&unset, GUEST_SCHED_FIFO) == GUEST_EINVAL);
    Require(pthread_attr_getschedpolicy_nid_postfix(&unset, &policy) == GUEST_EINVAL && policy == -1);

    Require(scePthreadAttrSetschedpolicy(&unset, GUEST_SCHED_FIFO) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetschedpolicy(&unset, &policy) == SCE_KERNEL_ERROR_EINVAL && policy == -1);

    PthreadAttr destroyed = nullptr;
    Require(pthread_attr_init_nid_postfix(&destroyed) == 0);
    Require(pthread_attr_destroy_nid_postfix(&destroyed) == 0);
    Require(pthread_attr_setschedpolicy_nid_postfix(&destroyed, GUEST_SCHED_FIFO) == GUEST_EINVAL);
    Require(pthread_attr_getschedpolicy_nid_postfix(&destroyed, &policy) == GUEST_EINVAL && policy == -1);

    Require(scePthreadAttrSetschedpolicy(&destroyed, GUEST_SCHED_FIFO) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetschedpolicy(&destroyed, &policy) == SCE_KERNEL_ERROR_EINVAL && policy == -1);

    PthreadAttr live = nullptr;
    Require(pthread_attr_init_nid_postfix(&live) == 0);
    Require(pthread_attr_getschedpolicy_nid_postfix(&live, nullptr) == GUEST_EINVAL);
    Require(scePthreadAttrGetschedpolicy(&live, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    GuardPage guard;
    void* const badPointers[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    for (void* pointer : badPointers) {
        auto* badAttr = static_cast<PthreadAttr*>(pointer);
        auto* badInt = static_cast<int*>(pointer);
        Require(pthread_attr_setschedpolicy_nid_postfix(badAttr, GUEST_SCHED_FIFO) == GUEST_EFAULT);
        Require(pthread_attr_getschedpolicy_nid_postfix(badAttr, &policy) == GUEST_EFAULT && policy == -1);
        Require(scePthreadAttrSetschedpolicy(badAttr, GUEST_SCHED_FIFO) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetschedpolicy(badAttr, &policy) == SCE_KERNEL_ERROR_EFAULT && policy == -1);

        Require(pthread_attr_getschedpolicy_nid_postfix(&live, badInt) == GUEST_EFAULT);
        Require(scePthreadAttrGetschedpolicy(&live, badInt) == SCE_KERNEL_ERROR_EFAULT);
    }

    Require(pthread_attr_getschedpolicy_nid_postfix(&live, &policy) == 0 && policy == GUEST_SCHED_FIFO);
    policy = -1;
    Require(scePthreadAttrGetschedpolicy(&live, &policy) == SCE_OK && policy == GUEST_SCHED_FIFO);

    for (int validPolicy : {GUEST_SCHED_FIFO, GUEST_SCHED_OTHER, GUEST_SCHED_RR}) {
        Require(pthread_attr_setschedpolicy_nid_postfix(&live, validPolicy) == 0);
        policy = -1;
        Require(pthread_attr_getschedpolicy_nid_postfix(&live, &policy) == 0 && policy == validPolicy);
    }

    const int invalidPolicies[] = {
        -2147483647 - 1, -100, -2, -1, 0, 4, 5, 10, 100, 0x7fffffff
    };
    for (int invalid : invalidPolicies) {
        Require(pthread_attr_setschedpolicy_nid_postfix(&live, invalid) == GUEST_ENOTSUP);
        policy = -1;
        Require(pthread_attr_getschedpolicy_nid_postfix(&live, &policy) == 0 && policy == GUEST_SCHED_RR);
    }

    Require(scePthreadAttrSetschedpolicy(&live, GUEST_SCHED_FIFO) == SCE_OK);
    policy = -1;
    Require(scePthreadAttrGetschedpolicy(&live, &policy) == SCE_OK && policy == GUEST_SCHED_FIFO);

    Require(scePthreadAttrSetschedpolicy(&live, GUEST_SCHED_RR) == SCE_OK);
    policy = -1;
    Require(scePthreadAttrGetschedpolicy(&live, &policy) == SCE_OK && policy == GUEST_SCHED_RR);

    const int sceRejectedPolicies[] = {
        -100, -1, 0, GUEST_SCHED_OTHER, 4, 5, 100
    };
    for (int rejected : sceRejectedPolicies) {
        Require(scePthreadAttrSetschedpolicy(&live, rejected) == SCE_KERNEL_ERROR_EOPNOTSUPP);
        policy = -1;
        Require(scePthreadAttrGetschedpolicy(&live, &policy) == SCE_OK && policy == GUEST_SCHED_RR);
    }

    Require(pthread_attr_destroy_nid_postfix(&live) == 0);
}

struct ThreadResult {
    int policy = -1;
    int priority = -1;
    int inheritSched = -1;
    int scope = -1;
};

struct ThreadContext {
    std::promise<ThreadResult> result;
    std::future<void> finish;
};

static void* APS5_VABI ChildWorker(void* arg) {
    auto* ctx = static_cast<ThreadContext*>(arg);
    ThreadResult res;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &res.policy, &param) == SCE_OK);
    res.priority = param.sched_priority;

    PthreadAttr queryAttr = nullptr;
    Require(scePthreadAttrInit(&queryAttr) == SCE_OK);
    Require(scePthreadAttrGet(scePthreadSelf(), &queryAttr) == SCE_OK);
    Require(scePthreadAttrGetinheritsched(&queryAttr, &res.inheritSched) == SCE_OK);
    Require(pthread_attr_getscope_nid_postfix(&queryAttr, &res.scope) == 0);
    Require(scePthreadAttrDestroy(&queryAttr) == SCE_OK);

    ctx->result.set_value(res);
    ctx->finish.wait();
    return nullptr;
}

static ThreadResult SpawnChild(int inheritSched, int policy, int priority, int scope) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, inheritSched) == 0);
    Require(pthread_attr_setschedpolicy_nid_postfix(&attr, policy) == 0);
    const KernelSchedParam param{priority};
    Require(pthread_attr_setschedparam_nid_postfix(&attr, &param) == 0);
    Require(pthread_attr_setscope_nid_postfix(&attr, scope) == 0);

    std::promise<void> finishPromise;
    ThreadContext ctx{{}, finishPromise.get_future()};
    auto resultFuture = ctx.result.get_future();

    Pthread child = nullptr;
    Require(scePthreadCreate(&child, &attr, ChildWorker, &ctx, "inherittest") == SCE_OK);

    ThreadResult res = resultFuture.get();

    PthreadAttr threadAttr = nullptr;
    Require(scePthreadAttrInit(&threadAttr) == SCE_OK);
    Require(pthread_attr_get_np_nid_postfix(child, &threadAttr) == 0);
    int threadInherit = -1;
    int threadPolicy = -1;
    int threadScope = -1;
    KernelSchedParam threadParam{};
    Require(pthread_attr_getinheritsched_nid_postfix(&threadAttr, &threadInherit) == 0);
    Require(pthread_attr_getschedpolicy_nid_postfix(&threadAttr, &threadPolicy) == 0);
    Require(pthread_attr_getschedparam_nid_postfix(&threadAttr, &threadParam) == 0);
    Require(pthread_attr_getscope_nid_postfix(&threadAttr, &threadScope) == 0);
    Require(threadInherit == res.inheritSched);
    Require(threadPolicy == res.policy);
    Require(threadParam.sched_priority == res.priority);
    Require(threadScope == res.scope);
    Require(scePthreadAttrDestroy(&threadAttr) == SCE_OK);

    finishPromise.set_value();
    Require(scePthreadJoin(child, nullptr) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    return res;
}

static void* APS5_VABI SchedExecutionRunner(void*) {
    const KernelSchedParam fifoPriority{650};
    Require(scePthreadSetschedparam(scePthreadSelf(), GUEST_SCHED_FIFO, &fifoPriority) == SCE_OK);

    ThreadResult explicitResult = SpawnChild(GUEST_EXPLICIT_SCHED, GUEST_SCHED_RR, 350, GUEST_SCOPE_PROCESS);
    Require(explicitResult.policy == GUEST_SCHED_RR);
    Require(explicitResult.priority == 350);
    Require(explicitResult.inheritSched == GUEST_EXPLICIT_SCHED);
    Require(explicitResult.scope == GUEST_SCOPE_PROCESS);

    ThreadResult inheritResult = SpawnChild(GUEST_INHERIT_SCHED, GUEST_SCHED_RR, 350, GUEST_SCOPE_PROCESS);
    Require(inheritResult.policy == GUEST_SCHED_FIFO);
    Require(inheritResult.priority == 650);
    Require(inheritResult.inheritSched == GUEST_INHERIT_SCHED);
    Require(inheritResult.scope == GUEST_SCOPE_SYSTEM);

    const KernelSchedParam rrPriority{500};
    Require(scePthreadSetschedparam(scePthreadSelf(), GUEST_SCHED_RR, &rrPriority) == SCE_OK);

    explicitResult = SpawnChild(GUEST_EXPLICIT_SCHED, GUEST_SCHED_FIFO, 260, GUEST_SCOPE_PROCESS);
    Require(explicitResult.policy == GUEST_SCHED_FIFO);
    Require(explicitResult.priority == 260);
    Require(explicitResult.inheritSched == GUEST_EXPLICIT_SCHED);
    Require(explicitResult.scope == GUEST_SCOPE_PROCESS);

    inheritResult = SpawnChild(GUEST_INHERIT_SCHED, GUEST_SCHED_FIFO, 260, GUEST_SCOPE_PROCESS);
    Require(inheritResult.policy == GUEST_SCHED_RR);
    Require(inheritResult.priority == 500);
    Require(inheritResult.inheritSched == GUEST_INHERIT_SCHED);
    Require(inheritResult.scope == GUEST_SCOPE_SYSTEM);

    return nullptr;
}

static void CheckInheritSchedVsExplicitSchedExecution() {
    Pthread runner = nullptr;
    Require(scePthreadCreate(&runner, nullptr, SchedExecutionRunner, nullptr, "schedrunner") == SCE_OK);
    Require(scePthreadJoin(runner, nullptr) == SCE_OK);
}

int main() {
    CheckInheritschedPointersAndFaults();
    CheckInheritschedDefaultsAndTransitions();
    CheckInheritschedInvalidValuesRejection();
    CheckSchedPolicyBoundsAndRejection();
    CheckInheritSchedVsExplicitSchedExecution();
    return 0;
}
