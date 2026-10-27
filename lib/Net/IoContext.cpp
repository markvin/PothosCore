// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Net/IoContext.hpp"
#include <Pothos/System/Logger.hpp>
#include <asio/executor_work_guard.hpp>
#include <mutex>
#include <thread>

namespace {

struct Service
{
    Service(void):
        work(asio::make_work_guard(context)),
        thread([this]{this->run();})
    {
        return;
    }

    //! Keep serving after an exception escapes a handler (it would otherwise end the process)
    void run(void)
    {
        while (true)
        {
            try
            {
                context.run();
                return; //stopped
            }
            catch (const std::exception &ex)
            {
                POTHOS_LOG_ERROR("Pothos.Net", "uncaught exception in a network handler: {}", ex.what());
            }
            catch (...)
            {
                POTHOS_LOG_ERROR("Pothos.Net", "uncaught exception in a network handler");
            }
        }
    }

    void stop(void)
    {
        work.reset();
        context.stop();
        if (not thread.joinable()) return;
        #ifdef _WIN32
        //joining from a static destructor of a DLL can deadlock on the loader lock
        thread.detach();
        #else
        thread.join();
        #endif
    }

    asio::io_context context;
    asio::executor_work_guard<asio::io_context::executor_type> work;
    std::thread thread;
};

//never destroyed: objects in other static destructors may still hold sockets of this context
Service *service = nullptr;
std::once_flag serviceOnce;

struct ShutdownGuard
{
    ~ShutdownGuard(void)
    {
        if (service != nullptr) service->stop();
    }
} shutdownGuard;

} //namespace

asio::io_context &Pothos::Net::ioContext(void)
{
    std::call_once(serviceOnce, []{service = new Service();});
    return service->context;
}
