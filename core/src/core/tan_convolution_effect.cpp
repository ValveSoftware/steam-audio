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

#if defined(IPL_USES_TRUEAUDIONEXT)

#include "tan_convolution_effect.h"

#include "array_math.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// TANConvolutionEffect
// --------------------------------------------------------------------------------------------------------------------

TANConvolutionEffect::TANConvolutionEffect()
{
    reset();
}

void TANConvolutionEffect::reset()
{
    mReset = true;
}

AudioEffectState TANConvolutionEffect::apply(const TANConvolutionEffectParams& params,
                                             const AudioBuffer& in,
                                             TANConvolutionMixer& mixer)
{
    if (mReset)
    {
        const_cast<TANDevice*>(params.tan)->reset(params.slot);
        mReset = false;
    }
    else
    {
        const_cast<TANDevice*>(params.tan)->setDry(params.slot, in);
    }

    // todo
    return AudioEffectState::TailComplete;
}

AudioEffectState TANConvolutionEffect::tail(TANConvolutionMixer& mixer)
{
    // todo
    return AudioEffectState::TailComplete;
}


// --------------------------------------------------------------------------------------------------------------------
// TANConvolutionMixer
// --------------------------------------------------------------------------------------------------------------------

TANConvolutionMixer::TANConvolutionMixer()
{
    reset();
}

void TANConvolutionMixer::reset()
{}

void TANConvolutionMixer::apply(const TANConvolutionMixerParams& params,
                                AudioBuffer& out)
{
    const_cast<TANDevice*>(params.tan)->process(out);
}

}

#endif
