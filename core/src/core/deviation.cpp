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

#include "deviation.h"
#include "math_functions.h"
#include "propagation_medium.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// DeviationModel
// --------------------------------------------------------------------------------------------------------------------

DeviationModel::DeviationModel()
    : mCallback(nullptr)
    , mUserData(nullptr)
{}

DeviationModel::DeviationModel(DeviationCallback callback, void* userData)
    : mCallback(callback)
    , mUserData(userData)
{}

bool DeviationModel::isDefault() const
{
    return (mCallback == nullptr);
}

float DeviationModel::evaluate(float angle, int band) const
{
    if (mCallback)
    {
        return mCallback(angle, band, mUserData);
    }
    else
    {
        return utdDeviation(angle, band);
    }
}

// The EQ coefficients for a given total deviation angle are calculated using UTD.
// See http://www-sop.inria.fr/reves/Nicolas.Tsingos/publis/sig2001.pdf.
float DeviationModel::utdDeviation(float angle, int band)
{
    auto cotf = [](const float theta)
    {
        auto tan_theta = tanf(theta);
        return 1.0f / tan_theta;
    };

    auto N_plus = [](const float n,
                     const float x)
    {
        if (x <= Math::kPi * (n - 1.0f))
            return 0.0f;
        else
            return 1.0f;
    };

    auto N_minus = [](const float n,
                      const float x)
    {
        if (x < Math::kPi * (1.0f - n))
            return -1.0f;
        else if (Math::kPi * (1.0f - n) <= x && x <= Math::kPi * (1.0f + n))
            return 0.0f;
        else
            return 1.0f;
    };

    auto a = [](const float n,
                const float beta,
                const float N)
    {
        auto cosine = cosf((Math::kPi * n * N) - (0.5f * beta));
        return 2.0f * cosine * cosine;
    };

    auto F = [](const float x)
    {
        auto e = std::polar(1.0f, 0.25f * Math::kPi * sqrtf(x / (x + 1.4f)));
        if (x < 0.8f)
        {
            auto term1 = sqrtf(Math::kPi * x);
            auto term2 = 1.0f - (sqrtf(x) / (0.7f * sqrtf(x) + 1.2f));
            return term1 * term2 * e;
        }
        else
        {
            auto term1 = 1.0f - (0.8f / ((x + 1.25f) * (x + 1.25f)));
            return term1 * e;
        }
    };

    auto sign = [](const float x)
    {
        return (x > 0.0f) ? 1 : -1;
    };

    const auto n = 2.0f;
    const auto alpha_i = 0.0f;
    const auto alpha_d = alpha_i + Math::kPi + angle;
    const auto L = 0.05f;

    const auto c = PropagationMedium::kSpeedOfSound;
    const auto f = (Bands::kLowCutoffFrequencies[band] + Bands::kHighCutoffFrequencies[band]) / 2.0f;
    const auto l = c / f;
    const auto k = (2.0f * Math::kPi) / l;

    auto e = std::polar(1.0f, -0.25f * Math::kPi);

    auto D0 = e / (2.0f * n * sqrtf(2.0f * Math::kPi * k));

    auto beta1 = alpha_d - alpha_i;
    auto beta2 = alpha_d - alpha_i;
    auto beta3 = alpha_d + alpha_i;
    auto beta4 = alpha_d + alpha_i;

    auto t1 = cotf((Math::kPi + beta1) / (2.0f * n));
    auto t2 = cotf((Math::kPi - beta2) / (2.0f * n));
    auto t3 = cotf((Math::kPi + beta3) / (2.0f * n));
    auto t4 = cotf((Math::kPi - beta4) / (2.0f * n));

    auto N1 = N_plus(n, beta1);
    auto N2 = N_minus(n, beta2);
    auto N3 = N_plus(n, beta3);
    auto N4 = N_minus(n, beta4);

    auto a1 = a(n, beta1, N1);
    auto a2 = a(n, beta2, N2);
    auto a3 = a(n, beta3, N3);
    auto a4 = a(n, beta4, N4);

    auto x1 = k * L * a1;
    auto x2 = k * L * a2;
    auto x3 = k * L * a3;
    auto x4 = k * L * a4;

    auto F1 = F(x1);
    auto F2 = F(x2);
    auto F3 = F(x3);
    auto F4 = F(x4);

    auto D1 = t1 * F1;
    auto D2 = t2 * F2;
    auto D3 = t3 * F3;
    auto D4 = t4 * F4;

    auto epsilon1 = beta1 - (2.0f * Math::kPi * n * N1) + Math::kPi;
    auto epsilon2 = -(beta2 - (2.0f * Math::kPi * n * N2) - Math::kPi);
    auto epsilon3 = beta3 - (2.0f * Math::kPi * n * N3) + Math::kPi;
    auto epsilon4 = -(beta4 - (2.0f * Math::kPi * n * N4) - Math::kPi);

    if (!Math::isFinite(t1))
    {
        D1 = n * e * ((sqrtf(2.0f * Math::kPi * k * L) * sign(epsilon1)) - (2 * k * L * epsilon1 * e));
    }
    if (!Math::isFinite(t2))
    {
        D2 = n * e * ((sqrtf(2.0f * Math::kPi * k * L) * sign(epsilon2)) - (2 * k * L * epsilon2 * e));
    }
    if (!Math::isFinite(t3))
    {
        D3 = n * e * ((sqrtf(2.0f * Math::kPi * k * L) * sign(epsilon3)) - (2 * k * L * epsilon3 * e));
    }
    if (!Math::isFinite(t4))
    {
        D4 = n * e * ((sqrtf(2.0f * Math::kPi * k * L) * sign(epsilon4)) - (2 * k * L * epsilon4 * e));
    }

    return abs(D0 * (D1 + D2 + D3 + D4));
}

}
