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

#include <thread>
#include <limits>

#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#define IDLE _mm_pause()
#elif __has_include(<intrin.h>)
#include <intrin.h>
#define IDLE _mm_pause()
#else
#define IDLE asm("nop")
#endif

namespace kls::thread {
    struct SpinWait {
    private:
        static constexpr unsigned int YieldThreshold = 10; // When to switch over to a true yield.
        static constexpr unsigned int Sleep0EveryHowManyYields = 5; // After how many yields should we Sleep(0)?
        static constexpr unsigned int DefaultSleep1Threshold = 20; // After how many yields should we Sleep(1) frequently?
        static bool IsSingleProcessor;
    public:
        static unsigned int OptimalMaxSpinWaitsPerSpinIteration;
        static unsigned int SpinCountForSpinBeforeWait;

        [[nodiscard]] unsigned int count() const noexcept { return m_count; }

        [[nodiscard]] bool will_yield() const noexcept { return m_count >= YieldThreshold || IsSingleProcessor; }

        void once() noexcept { once_core(DefaultSleep1Threshold); }

        void once(unsigned int threshold) noexcept {
            if (threshold >= 0 && threshold < YieldThreshold) { threshold = YieldThreshold; }
            once_core(threshold);
        }

        void reset() noexcept { m_count = 0; }

    private:
        unsigned int m_count = 0;

        void once_core(const unsigned int threshold) noexcept {
            if ((m_count >= YieldThreshold
                && ((m_count >= threshold && threshold >= 0) || (m_count - YieldThreshold) % 2 == 0))
                || IsSingleProcessor) {
                if (m_count >= threshold && threshold >= 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else {
                    const int yieldsSoFar = m_count >= YieldThreshold ? (m_count - YieldThreshold) / 2 : m_count;
                    if ((yieldsSoFar % Sleep0EveryHowManyYields) == (Sleep0EveryHowManyYields - 1)) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(0));
                    }
                    else { std::this_thread::yield(); }
                }
            }
            else {
                auto n = OptimalMaxSpinWaitsPerSpinIteration;
                if (m_count <= 30 && (1u << m_count) < n) { n = 1u << m_count; }
                spin(n);
            }

            // Finally, increment our spin counter.
            m_count = (m_count == std::numeric_limits<int>::max() ? YieldThreshold : m_count + 1);
        }

        static void spin(const unsigned int iterations) noexcept { for (auto i = 0u; i < iterations; ++i) { IDLE; } }
    };
}
