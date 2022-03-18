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

#include <atomic>
#include "SpinWait.h"
#include "kls/Object.h"

namespace kls::thread {
    class SpinLock: public AddressSensitive {
    public:
        void lock() noexcept {
            for (;;) {
                auto expect = false;
                if (mLock.compare_exchange_strong(expect, true, std::memory_order_acquire)) return;
                wait_for_lock();
            }
        }

        void unlock() noexcept { mLock.store(false, std::memory_order_release); }

    private:
        void wait_for_lock() const noexcept {
            SpinWait spinner{};
            while (mLock.load(std::memory_order_relaxed)) { spinner.once(); }
        }

        std::atomic_bool mLock = { false };
    };
}
