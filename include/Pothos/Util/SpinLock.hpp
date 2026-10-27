///
/// \file Util/SpinLock.hpp
///
/// A lightweight lock: a short spin, then it sleeps until unlocked.
///
/// \copyright
/// Copyright (c) 2014-2017 Josh Blum
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <Pothos/Config.hpp>
#include <atomic>
#include <mutex>

namespace Pothos {
namespace Util {

/*!
 * A lightweight lock on std::atomic_flag: lock() spins briefly, then waits
 * with std::atomic_flag::wait until unlock() notifies, so a contended lock
 * does not burn CPU time. The core itself uses std::mutex; this class stays
 * for code that already uses it.
 *
 * This lock can be used with std::lock_guard<Pothos::Util::SpinLock>
 */
class POTHOS_API SpinLock
{
public:

    //! Create a new unlocked spin lock
    SpinLock(void);

    //! Try to lock, return true for lock
    bool try_lock(void) noexcept;

    //! Lock the spin lock, block if already locked
    void lock(void) noexcept;

    //! Unlock the spin lock (should be already locked)
    void unlock(void) noexcept;

private:
    std::atomic_flag _lock;
};

} //namespace Util
} //namespace Pothos

inline Pothos::Util::SpinLock::SpinLock(void)
{
    this->unlock();
}

inline bool Pothos::Util::SpinLock::try_lock(void) noexcept
{
    return not _lock.test_and_set(std::memory_order_acquire);
}

inline void Pothos::Util::SpinLock::lock(void) noexcept
{
    //a short spin for the common brief hold, then sleep until unlocked
    for (int i = 0; i < 64; i++)
    {
        if (this->try_lock()) return;
    }
    while (not this->try_lock()) _lock.wait(true, std::memory_order_relaxed);
}

inline void Pothos::Util::SpinLock::unlock(void) noexcept
{
    _lock.clear(std::memory_order_release);
    _lock.notify_one();
}
