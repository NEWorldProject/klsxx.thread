/*
* Copyright (c) 2022 DWVoid and Infinideastudio Team
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include <chrono>

#if __has_include(<mach/semaphore.h>)
#include <mach/semaphore.h>
#include <mach/mach_init.h>
#include <mach/task.h>

namespace kls::thread {
    class Semaphore {
    public:
        explicit Semaphore() noexcept
            :handle(New()) { }

        ~Semaphore() { Release(handle); }

        void wait() noexcept {
            while (semaphore_wait(handle) != KERN_SUCCESS) {}
        }

        template <class Rep, class Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& relTime) noexcept {
            return timed_wait(std::chrono::duration_cast<std::chrono::milliseconds>(relTime).count());
        }

        template <class Clock, class Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration>& absTime) noexcept {
            return wait_for(absTime - Clock::now());
        }

        void signal() noexcept { aemaphore_signal(handle); }
    private:
        static semaphore_t New() noexcept {
            semaphore_t ret;
            semaphore_create(mach_task_self(), &ret, SYNC_POLICY_FIFO, 0);
            return ret;
        }

        bool timed_wait(const long long ms) noexcept {
            mach_timespec_t ts;
            ts.tv_sec = ms / 1000;
            ts.tv_nsec = (ms % 1000) * 1000000;
            bool wait = true;
            const auto err = semaphore_timedwait(sem, ts);
            return err == KERN_SUCCESS;
        }

        static void Release(Semaphore_t sem) noexcept {
            semaphore_destroy(mach_task_self(), sem);
        }

        Semaphore_t handle;
    };
}

#elif __has_include(<Windows.h>)

#include "kls/hal/System.h"

namespace kls::thread {
    class Semaphore {
    public:
        Semaphore() noexcept
            : handle(CreateSemaphore(nullptr, 0, MAXLONG, nullptr)) {}

        ~Semaphore() noexcept { CloseHandle(handle); }

        void wait() noexcept { WaitForSingleObject(handle, INFINITE); }

        template <class Rep, class Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& relTime) noexcept {
            return timed_wait(std::chrono::duration_cast<std::chrono::milliseconds>(relTime).count());
        }

        template <class Clock, class Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration>& absTime) noexcept {
            return wait_for(absTime - Clock::now());
        }

        void signal() noexcept {
            LONG last;
            ReleaseSemaphore(handle, 1, &last);
        }

    private:
        HANDLE handle;

        bool timed_wait(const long long ms) noexcept {
            return WaitForSingleObject(handle, static_cast<DWORD>(ms)) != WAIT_TIMEOUT;
        }
    };
}

#elif __has_include(<semaphore.h>)

#include <semaphore.h>

namespace kls::thread {
    class Semaphore {
    public:
        Semaphore() noexcept { sem_init(&mSem, 0, 0); }

        ~Semaphore() noexcept { sem_destroy(&mSem); }

        void wait() noexcept { sem_wait(&mSem); }

        template<class Rep, class Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& relTime) noexcept {
            return wait_until(relTime + std::chrono::system_clock::now());
        }

        template<class Clock, class Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration>& absTime) noexcept {
            const auto spec = Timespec(absTime);
            return (sem_timedwait(&mSem, &spec) == 0);
        }

        void signal() noexcept { sem_post(&mSem); }

    private:
        sem_t mSem{};

        template<class Clock, class Duration>
        timespec Timespec(const std::chrono::time_point<Clock, Duration>& tp) {
            using namespace std::chrono;
            auto secs = time_point_cast<seconds>(tp);
            auto ns = time_point_cast<nanoseconds>(tp) - time_point_cast<nanoseconds>(secs);
            return timespec{ secs.time_since_epoch().count(), ns.count() };
        }
    };
}

#else

namespace kls::thread {
    class Semaphore {
    public:
        Semaphore() noexcept {}

        ~Semaphore() noexcept {}

        void wait() noexcept {}

        void signal() noexcept {}
    };
}

# error "No Adaquate Semaphore Supported to be adapted from"
#endif