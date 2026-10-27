// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Util/SpinLock.hpp>
#include <chrono>
#include <ctime>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

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
    lock.lock();
    const auto cpuStart = std::clock();
    for (int t = 0; t < 4; t++) threads.emplace_back([&]
    {
        std::lock_guard<Pothos::Util::SpinLock> guard(lock);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto cpuUsed = double(std::clock() - cpuStart) / CLOCKS_PER_SEC;
    lock.unlock();
    for (auto &thread : threads) thread.join();
    std::cout << "CPU time of the waiters: " << cpuUsed << " s" << std::endl;
    POTHOS_TEST_LT(cpuUsed, 0.1);
}
