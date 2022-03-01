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

#include <cmath>
#include "kls/hal/Perf.h"
#include "kls/thread/SpinWait.h"

namespace {
    constexpr unsigned int MinNsPerNormalizedYield = 37; // measured typically 37-46 on post-Skylake
    constexpr unsigned int NsPerOptimalMaxSpinIterationDuration = 272;
    // approx. 900 cycles, measured 281 on pre-Skylake, 263 on post-Skylake

    unsigned int InitializeYieldProcessorNormalized() noexcept {
        // Intel pre-Skylake processor: measured typically 14-17 cycles per yield
        // Intel post-Skylake processor: measured typically 125-150 cycles per yield
        const auto MeasureDurationMs = 10;
        const auto NsPerSecond = 1000 * 1000 * 1000;

        auto ticksPerSecond = kls::hal::performance::frequency();
        if (!ticksPerSecond || ticksPerSecond < 1000 / MeasureDurationMs) {
            // High precision clock not available or clock resolution is too low, resort to defaults
            return 7;
        }

        // Measure the nanosecond delay per yield
        auto measureDurationTicks = *ticksPerSecond / (1000 / MeasureDurationMs);
        unsigned int yieldCount = 0;
        int64_t startTicks = *kls::hal::performance::counter();
        int64_t elapsedTicks;
        do {
            // On some systems, querying the high performance counter has relatively significant overhead. Do enough yields to mask
            // the timing overhead. Assuming one yield has a delay of MinNsPerNormalizedYield, 1000 yields would have a delay in the
            // low microsecond range.
            for (auto i = 0; i < 1000; ++i) { IDLE; }
            yieldCount += 1000;
            elapsedTicks = *kls::hal::performance::counter() - startTicks;
        }
        while (elapsedTicks < measureDurationTicks);
        auto nsPerYield = static_cast<double>(elapsedTicks) * NsPerSecond / 
            (static_cast<double>(yieldCount) * *ticksPerSecond);
        if (nsPerYield < 1) { nsPerYield = 1; }

        // Calculate the number of yields required to span the duration of a normalized yield. Since nsPerYield is at least 1, this
        // value is naturally limited to MinNsPerNormalizedYield.
        int yieldsPerNormalizedYield = lround(MinNsPerNormalizedYield / nsPerYield);
        if (yieldsPerNormalizedYield < 1) { yieldsPerNormalizedYield = 1; }

        // Calculate the maximum number of yields that would be optimal for a late spin iteration. Typically, we would not want to
        // spend excessive amounts of time (thousands of cycles) doing only YieldProcessor, as SwitchToThread/Sleep would do a
        // better job of allowing other work to run.
        int optimalMaxNormalizedYieldsPerSpinIteration = lround(
                NsPerOptimalMaxSpinIterationDuration / (yieldsPerNormalizedYield * nsPerYield));
        if (optimalMaxNormalizedYieldsPerSpinIteration < 1) { optimalMaxNormalizedYieldsPerSpinIteration = 1; }

        return optimalMaxNormalizedYieldsPerSpinIteration;
    }
}

namespace kls::thread {
    bool SpinWait::IsSingleProcessor = std::thread::hardware_concurrency() == 1;
    unsigned int SpinWait::SpinCountForSpinBeforeWait = IsSingleProcessor ? 1 : 35;
    unsigned int SpinWait::OptimalMaxSpinWaitsPerSpinIteration = InitializeYieldProcessorNormalized();
}
