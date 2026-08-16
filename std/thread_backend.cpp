// ALU Standard Library: thread_backend.cpp
// C++ backend implementing threading and synchronization primitives.
//
// ABI Note: The ALU compiler does NOT append the hidden `void** __alu_err`
// parameter to `extern routine` declarations. Therefore every extern "C"
// function here must match the signature the compiler actually emits.
// For example, `extern routine thread_spawn(ptr<routine> fn) -> int;`
// becomes `extern "C" int thread_spawn(void* fn_ptr)` — no err pointer.

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>

// ─── Global registries ───────────────────────────────────────────────────────

static std::unordered_map<int, std::unique_ptr<std::thread>>              thread_map;
static std::unordered_map<int, std::unique_ptr<std::mutex>>               mutex_map;
static std::unordered_map<int, std::unique_ptr<std::condition_variable>>  cond_map;
static std::unordered_map<int, std::unique_ptr<std::shared_mutex>>        rwlock_map;

// Barrier: implemented with mutex + condvar + counter since std::barrier
// requires C++20 and may not be available everywhere.
struct AluBarrier {
    std::mutex              mtx;
    std::condition_variable cv;
    int                     threshold;
    int                     count;
    int                     generation;
};
static std::unordered_map<int, std::unique_ptr<AluBarrier>> barrier_map;

// Counting semaphore: implemented with mutex + condvar for portability.
struct AluSemaphore {
    std::mutex              mtx;
    std::condition_variable cv;
    int                     count;
};
static std::unordered_map<int, std::unique_ptr<AluSemaphore>> semaphore_map;

// A single global lock protects all registry mutations.
static std::mutex global_lock;

// Monotonic ID generators.
static std::atomic<int> next_thread_id{1};
static std::atomic<int> next_mutex_id{1};
static std::atomic<int> next_cond_id{1};
static std::atomic<int> next_rwlock_id{1};
static std::atomic<int> next_barrier_id{1};
static std::atomic<int> next_semaphore_id{1};

// Thread-local ID visible from ALU code.
static std::atomic<int>        next_logical_thread_id{1};
static thread_local int        tl_thread_id = 0;

// ALU routine signature: void routine(i8** __alu_err)
typedef void (*AluRoutinePtr)(void**);

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 1: Thread primitives (std/thread.alu)
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" int thread_spawn(void* fn_ptr) {
    if (!fn_ptr) return 0;
    AluRoutinePtr routine = (AluRoutinePtr)fn_ptr;

    int id = next_thread_id++;
    auto t = std::make_unique<std::thread>([routine, id]() {
        // Assign a logical thread ID to the child.
        tl_thread_id = next_logical_thread_id++;
        void* child_err = nullptr;
        routine(&child_err);
    });

    std::lock_guard<std::mutex> lock(global_lock);
    thread_map[id] = std::move(t);
    return id;
}

extern "C" void thread_join(int thread_id) {
    std::thread* t = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = thread_map.find(thread_id);
        if (it != thread_map.end()) {
            t = it->second.get();
        }
    }

    if (t && t->joinable()) {
        t->join();

        std::lock_guard<std::mutex> lock(global_lock);
        thread_map.erase(thread_id);
    }
}

extern "C" void thread_detach(int thread_id) {
    std::lock_guard<std::mutex> lock(global_lock);
    auto it = thread_map.find(thread_id);
    if (it != thread_map.end()) {
        if (it->second->joinable()) {
            it->second->detach();
        }
        thread_map.erase(it);
    }
}

extern "C" int thread_current_id() {
    // Lazily assign an ID the first time this is called on the main thread.
    if (tl_thread_id == 0) {
        tl_thread_id = next_logical_thread_id++;
    }
    return tl_thread_id;
}

extern "C" void thread_yield() {
    std::this_thread::yield();
}

extern "C" int thread_hardware_concurrency() {
    unsigned n = std::thread::hardware_concurrency();
    return (n == 0) ? 1 : static_cast<int>(n);
}

extern "C" void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 2: Mutex (std/sync.alu)
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" int sync_mutex_new() {
    int id = next_mutex_id++;
    auto m = std::make_unique<std::mutex>();

    std::lock_guard<std::mutex> lock(global_lock);
    mutex_map[id] = std::move(m);
    return id;
}

extern "C" void sync_mutex_lock(int mutex_id) {
    std::mutex* m = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = mutex_map.find(mutex_id);
        if (it != mutex_map.end()) {
            m = it->second.get();
        }
    }
    if (m) {
        m->lock();
    }
}

extern "C" void sync_mutex_unlock(int mutex_id) {
    std::mutex* m = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = mutex_map.find(mutex_id);
        if (it != mutex_map.end()) {
            m = it->second.get();
        }
    }
    if (m) {
        m->unlock();
    }
}

extern "C" int sync_mutex_try_lock(int mutex_id) {
    std::mutex* m = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = mutex_map.find(mutex_id);
        if (it != mutex_map.end()) {
            m = it->second.get();
        }
    }
    if (m) {
        return m->try_lock() ? 1 : 0;
    }
    return 0;
}

extern "C" void sync_mutex_destroy(int mutex_id) {
    std::lock_guard<std::mutex> lock(global_lock);
    mutex_map.erase(mutex_id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 3: Condition Variables (std/sync.alu)
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" int sync_cond_new() {
    int id = next_cond_id++;
    auto c = std::make_unique<std::condition_variable>();

    std::lock_guard<std::mutex> lock(global_lock);
    cond_map[id] = std::move(c);
    return id;
}

// condition_variable::wait requires a unique_lock.  We construct one with
// std::adopt_lock (the caller already holds the mutex) and release()
// ownership afterwards so the caller can continue to use the mutex.
extern "C" void sync_cond_wait(int cond_id, int mutex_id) {
    std::condition_variable* c = nullptr;
    std::mutex* m = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it_c = cond_map.find(cond_id);
        if (it_c != cond_map.end()) c = it_c->second.get();

        auto it_m = mutex_map.find(mutex_id);
        if (it_m != mutex_map.end()) m = it_m->second.get();
    }

    if (c && m) {
        std::unique_lock<std::mutex> lk(*m, std::adopt_lock);
        c->wait(lk);
        // Release ownership so the caller's subsequent unlock doesn't fail.
        lk.release();
    }
}

// Returns 1 if signaled before timeout, 0 on timeout.
extern "C" int sync_cond_wait_timeout(int cond_id, int mutex_id, int timeout_ms) {
    std::condition_variable* c = nullptr;
    std::mutex* m = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it_c = cond_map.find(cond_id);
        if (it_c != cond_map.end()) c = it_c->second.get();

        auto it_m = mutex_map.find(mutex_id);
        if (it_m != mutex_map.end()) m = it_m->second.get();
    }

    if (c && m) {
        std::unique_lock<std::mutex> lk(*m, std::adopt_lock);
        auto status = c->wait_for(lk, std::chrono::milliseconds(timeout_ms));
        lk.release();
        return (status == std::cv_status::no_timeout) ? 1 : 0;
    }
    return 0;
}

extern "C" void sync_cond_signal(int cond_id) {
    std::condition_variable* c = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = cond_map.find(cond_id);
        if (it != cond_map.end()) c = it->second.get();
    }
    if (c) {
        c->notify_one();
    }
}

extern "C" void sync_cond_broadcast(int cond_id) {
    std::condition_variable* c = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = cond_map.find(cond_id);
        if (it != cond_map.end()) c = it->second.get();
    }
    if (c) {
        c->notify_all();
    }
}

extern "C" void sync_cond_destroy(int cond_id) {
    std::lock_guard<std::mutex> lock(global_lock);
    cond_map.erase(cond_id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 4: Read-Write Lock (std/sync.alu)
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" int sync_rwlock_new() {
    int id = next_rwlock_id++;
    auto rw = std::make_unique<std::shared_mutex>();

    std::lock_guard<std::mutex> lock(global_lock);
    rwlock_map[id] = std::move(rw);
    return id;
}

extern "C" void sync_rwlock_read_lock(int rwlock_id) {
    std::shared_mutex* rw = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = rwlock_map.find(rwlock_id);
        if (it != rwlock_map.end()) rw = it->second.get();
    }
    if (rw) {
        rw->lock_shared();
    }
}

extern "C" void sync_rwlock_read_unlock(int rwlock_id) {
    std::shared_mutex* rw = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = rwlock_map.find(rwlock_id);
        if (it != rwlock_map.end()) rw = it->second.get();
    }
    if (rw) {
        rw->unlock_shared();
    }
}

extern "C" void sync_rwlock_write_lock(int rwlock_id) {
    std::shared_mutex* rw = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = rwlock_map.find(rwlock_id);
        if (it != rwlock_map.end()) rw = it->second.get();
    }
    if (rw) {
        rw->lock();
    }
}

extern "C" void sync_rwlock_write_unlock(int rwlock_id) {
    std::shared_mutex* rw = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = rwlock_map.find(rwlock_id);
        if (it != rwlock_map.end()) rw = it->second.get();
    }
    if (rw) {
        rw->unlock();
    }
}

extern "C" void sync_rwlock_destroy(int rwlock_id) {
    std::lock_guard<std::mutex> lock(global_lock);
    rwlock_map.erase(rwlock_id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 5: Barrier (std/sync.alu)
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" int sync_barrier_new(int count) {
    int id = next_barrier_id++;
    auto b = std::make_unique<AluBarrier>();
    b->threshold  = count;
    b->count      = 0;
    b->generation = 0;

    std::lock_guard<std::mutex> lock(global_lock);
    barrier_map[id] = std::move(b);
    return id;
}

// Blocks until `count` threads have called barrier_wait on this barrier.
// Returns 1 for the last thread to arrive (the "serial thread"), 0 for others.
extern "C" int sync_barrier_wait(int barrier_id) {
    AluBarrier* b = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = barrier_map.find(barrier_id);
        if (it != barrier_map.end()) b = it->second.get();
    }
    if (!b) return 0;

    std::unique_lock<std::mutex> lk(b->mtx);
    int gen = b->generation;
    b->count++;
    if (b->count >= b->threshold) {
        // Last thread: reset and wake everyone.
        b->count = 0;
        b->generation++;
        lk.unlock();
        b->cv.notify_all();
        return 1;  // serial thread
    } else {
        b->cv.wait(lk, [b, gen]() { return b->generation != gen; });
        return 0;
    }
}

extern "C" void sync_barrier_destroy(int barrier_id) {
    std::lock_guard<std::mutex> lock(global_lock);
    barrier_map.erase(barrier_id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 6: Counting Semaphore (std/sync.alu)
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" int sync_semaphore_new(int initial) {
    int id = next_semaphore_id++;
    auto s = std::make_unique<AluSemaphore>();
    s->count = initial;

    std::lock_guard<std::mutex> lock(global_lock);
    semaphore_map[id] = std::move(s);
    return id;
}

// Decrements the semaphore. Blocks if the count is zero.
extern "C" void sync_semaphore_wait(int sem_id) {
    AluSemaphore* s = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = semaphore_map.find(sem_id);
        if (it != semaphore_map.end()) s = it->second.get();
    }
    if (!s) return;

    std::unique_lock<std::mutex> lk(s->mtx);
    s->cv.wait(lk, [s]() { return s->count > 0; });
    s->count--;
}

// Increments the semaphore, potentially unblocking a waiter.
extern "C" void sync_semaphore_post(int sem_id) {
    AluSemaphore* s = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = semaphore_map.find(sem_id);
        if (it != semaphore_map.end()) s = it->second.get();
    }
    if (!s) return;

    {
        std::lock_guard<std::mutex> lk(s->mtx);
        s->count++;
    }
    s->cv.notify_one();
}

// Non-blocking try: returns 1 if acquired, 0 if not.
extern "C" int sync_semaphore_try_wait(int sem_id) {
    AluSemaphore* s = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_lock);
        auto it = semaphore_map.find(sem_id);
        if (it != semaphore_map.end()) s = it->second.get();
    }
    if (!s) return 0;

    std::lock_guard<std::mutex> lk(s->mtx);
    if (s->count > 0) {
        s->count--;
        return 1;
    }
    return 0;
}

extern "C" void sync_semaphore_destroy(int sem_id) {
    std::lock_guard<std::mutex> lock(global_lock);
    semaphore_map.erase(sem_id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 7: Atomic Operations (std/sync.alu)
// ═══════════════════════════════════════════════════════════════════════════════
//
// These operate on plain int* (i32*) pointers using platform intrinsics.
// On MSVC we use _Interlocked* ; on GCC/Clang we use __atomic builtins.
// The caller is responsible for ensuring valid, aligned pointers.

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_InterlockedExchangeAdd, _InterlockedExchange, _InterlockedCompareExchange)

extern "C" int sync_atomic_load(volatile int* ptr) {
    // A simple volatile read + compiler barrier is sequentially consistent
    // on x86/x64.  _InterlockedOr with 0 is a full fence alternative.
    return _InterlockedOr((volatile long*)ptr, 0);
}

extern "C" void sync_atomic_store(volatile int* ptr, int val) {
    _InterlockedExchange((volatile long*)ptr, (long)val);
}

extern "C" int sync_atomic_add(volatile int* ptr, int val) {
    return (int)_InterlockedExchangeAdd((volatile long*)ptr, (long)val);
}

extern "C" int sync_atomic_sub(volatile int* ptr, int val) {
    return (int)_InterlockedExchangeAdd((volatile long*)ptr, (long)(-val));
}

// Returns the old value.  Stores `desired` only if *ptr == expected.
extern "C" int sync_atomic_cas(volatile int* ptr, int expected, int desired) {
    return (int)_InterlockedCompareExchange((volatile long*)ptr,
                                            (long)desired, (long)expected);
}

extern "C" int sync_atomic_exchange(volatile int* ptr, int val) {
    return (int)_InterlockedExchange((volatile long*)ptr, (long)val);
}

#else
// GCC / Clang path
extern "C" int sync_atomic_load(int* ptr) {
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

extern "C" void sync_atomic_store(int* ptr, int val) {
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
}

extern "C" int sync_atomic_add(int* ptr, int val) {
    return __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST);
}

extern "C" int sync_atomic_sub(int* ptr, int val) {
    return __atomic_fetch_sub(ptr, val, __ATOMIC_SEQ_CST);
}

extern "C" int sync_atomic_cas(int* ptr, int expected, int desired) {
    int old = expected;
    __atomic_compare_exchange_n(ptr, &old, desired, false,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old;
}

extern "C" int sync_atomic_exchange(int* ptr, int val) {
    return __atomic_exchange_n(ptr, val, __ATOMIC_SEQ_CST);
}
#endif
