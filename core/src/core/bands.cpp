//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "bands.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Bands
// --------------------------------------------------------------------------------------------------------------------

namespace Bands
{
#if defined(IPL_ENABLE_OCTAVE_BANDS)
    const float kLowCutoffFrequencies[kNumBands] = {11.049, 22.097, 44.194, 88.388, 176.78, 353.55, 707.11, 1414.2, 2828.4, 5656.9, 11314};
    const float kHighCutoffFrequencies[kNumBands] = {22.097, 44.194, 88.388, 176.78, 353.55, 707.11, 1414.2, 2828.4, 5656.9, 11314, 22627};
#else
    const float kLowCutoffFrequencies[kNumBands] = { 0.0f, 800.0f, 8000.0f };
    const float kHighCutoffFrequencies[kNumBands] = { 800.0f, 8000.0f, 22000.0f };
#endif

    // These values are obtained for band-pass filters by calling spectrumPeak() on an 8th order IIR filter, with a
    // sampling rate of 44.1 kHz. For low-pass and high-pass filters, these values are not used (and are set to 1).
    // These values are only used for 8th-order filters.
#if defined(IPL_ENABLE_OCTAVE_BANDS)
    const float kBandPassNorm[kNumBands] = {
        1.0f,
        0.198080f,
        0.641167f,
        8.377070f,
        1.272342f,
        0.944063f,
        0.941724f,
        0.943419f,
        0.949938f,
        0.972749f,
        1.0f
    };
#else
    const float kBandPassNorm[kNumBands] = { 1.0f, 0.999937f, 1.0f };
#endif
}

}