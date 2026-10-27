// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Util/SpinLock.hpp>
#include <atomic>
#include <chrono>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

//CPU time of the calling thread in seconds
//(std::clock() is process time on POSIX and wall time on Windows)
static double threadCpuSeconds(void)
{
#ifdef _WIN32
    FILETIME creation, exit, kernel, user;
    GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user);
    const auto ticks = [](const FILETIME &t){return (ULONGLONG(t.dwHighDateTime) << 32) | t.dwLowDateTime;};
    return double(ticks(kernel) + ticks(user))*1e-7; //100 ns units
#else
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec)*1e-9;
#endif
}

POTHOS_TEST_BLOCK("/util/tests", test_spinlock_contention)
{
    //mutual exclusion under contention
    Pothos::Util::SpinLock lock;
    long counter = 0;
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++) threads.emplace_back([&]
    {
        for (int i = 0; i < 100000; i++)
        {
            std::lock_guard<Pothos::Util::SpinLock> guard(lock);
            counter++;
        }
    });
    for (auto &thread : threads) thread.join();
    POTHOS_TEST_EQUAL(counter, 800000);

    //try_lock on a held lock
    lock.lock();
    POTHOS_TEST_TRUE(not lock.try_lock());
    lock.unlock();
    POTHOS_TEST_TRUE(lock.try_lock());
    lock.unlock();

    //waiting threads sleep instead of spinning: 4 waiters during a 200 ms hold
    //burn far less than the 800 ms of CPU time that spinning would
    threads.clear();
    //another busy thread in the process must not count against the waiters
    std::atomic_bool noiseDone(false);
    std::thread noise([&]{while (not noiseDone.load()){}});
    lock.lock();
    std::atomic<double> cpuUsed(0.0);
    for (int t = 0; t < 4; t++) threads.emplace_back([&]
    {
        const auto cpuStart = threadCpuSeconds();
        std::lock_guard<Pothos::Util::SpinLock> guard(lock);
        cpuUsed += threadCpuSeconds() - cpuStart;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    lock.unlock();
    for (auto &thread : threads) thread.join();
    noiseDone = true;
    noise.join();
    std::cout << "CPU time of the waiters: " << cpuUsed.load() << " s" << std::endl;
    POTHOS_TEST_LT(cpuUsed.load(), 0.1);
}
