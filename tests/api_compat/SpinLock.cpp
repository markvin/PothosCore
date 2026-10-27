// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//the locking pattern of comms/mac/SimpleLlc.cpp
#include <Pothos/Util/SpinLock.hpp>
#include <mutex> //lock_guard
#include <deque>

class PacketQueue
{
public:
    void push(const int packet)
    {
        std::lock_guard<Pothos::Util::SpinLock> lock(_lock);
        _packets.push_back(packet);
    }

    bool pop(int &packet)
    {
        std::lock_guard<Pothos::Util::SpinLock> lock(_lock);
        if (_packets.empty()) return false;
        packet = _packets.front();
        _packets.pop_front();
        return true;
    }

private:
    Pothos::Util::SpinLock _lock;
    std::deque<int> _packets;
};

bool apiCompatSpinLock(void)
{
    PacketQueue queue;
    queue.push(7);
    int packet = 0;
    return queue.pop(packet) and packet == 7 and not queue.pop(packet);
}
