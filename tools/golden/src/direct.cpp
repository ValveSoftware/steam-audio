// Golden data for stage 4: distance_attenuation.{h,cpp}, air_absorption.{h,cpp}, directivity.{h,cpp},
// gain_effect.{h,cpp}, eq_effect.{h,cpp}, direct_effect.{h,cpp}.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/direct.rs replays the
// same order. EQEffect filters with IIRFilterer, which picks its block algorithm from Context::sSIMDLevel when it is
// constructed: the effects are built with the AVX level, the path the port follows (risk R8; needs an AVX CPU).

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#include <air_absorption.h>
#include <context.h>
#include <direct_effect.h>
#include <directivity.h>
#include <distance_attenuation.h>
#include <eq_effect.h>
#include <gain_effect.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

Vector3f drawVector(Lcg& lcg)
{
    const float x = lcg.nextSigned();
    const float y = lcg.nextSigned();
    const float z = lcg.nextSigned();
    return Vector3f(x, y, z);
}

// Uniform in [lo, hi).
float drawRange(Lcg& lcg, float lo, float hi)
{
    const float u = lcg.nextSigned();
    return lo + (hi - lo) * 0.5f * (u + 1.0f);
}

void fill(Lcg& lcg, AudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels(); ++i)
    {
        for (int j = 0; j < buffer.numSamples(); ++j)
            buffer[i][j] = lcg.nextSigned();
    }
}

void append(std::vector<float>& out, const AudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels(); ++i)
        out.insert(out.end(), buffer[i], buffer[i] + buffer.numSamples());
}

// Distances evaluated before the random ones.
const float kFixedDistances[] = {0.0f, -0.0f, 0.5f, 1.0f, 1.5f, -1.0f, 1e-3f, 100.0f, 1e6f, 1e-30f};
constexpr int kNumRandomDistances = 200;

std::vector<float> distances(Lcg& lcg)
{
    std::vector<float> d(std::begin(kFixedDistances), std::end(kFixedDistances));
    for (int i = 0; i < kNumRandomDistances; ++i)
        d.push_back(drawRange(lcg, -5.0f, 400.0f));
    return d;
}

// --------------------------------------------------------------------------------------------------------------------
// DistanceAttenuationModel
// --------------------------------------------------------------------------------------------------------------------

float IPL_CALLBACK distanceCallback(float distance, void* userData)
{
    const auto k = *static_cast<const float*>(userData);
    return 1.0f / (1.0f + k * distance * distance);
}

float gDistanceK1 = 0.3f;
float gDistanceK2 = 0.05f;

// Models in this order: default, explicit default, inverse distance from 0.5, 3.7 and 0, a callback (twice, the same
// function and user data), and the callback with other user data.
std::vector<DistanceAttenuationModel> distanceModels()
{
    return {
        DistanceAttenuationModel{},
        DistanceAttenuationModel(1.0f, nullptr, nullptr),
        DistanceAttenuationModel(0.5f, nullptr, nullptr),
        DistanceAttenuationModel(3.7f, nullptr, nullptr),
        DistanceAttenuationModel(0.0f, nullptr, nullptr),
        DistanceAttenuationModel(1.0f, distanceCallback, &gDistanceK1),
        DistanceAttenuationModel(1.0f, distanceCallback, &gDistanceK1),
        DistanceAttenuationModel(1.0f, distanceCallback, &gDistanceK2),
    };
}

constexpr int kSamplingRates[] = {44100, 48000, 24000};
constexpr int kCurveLength = 1001;

// (from, to) model indices for the correction curves.
constexpr int kCurvePairs[][2] = {{0, 2}, {0, 5}, {3, 0}, {7, 2}, {2, 3}};

template <typename Model>
void writeIdentity(const std::filesystem::path& out, const std::vector<Model>& models)
{
    std::vector<int32_t> isDefault;
    std::vector<int32_t> equal;
    for (const auto& a : models)
    {
        isDefault.push_back(a.isDefault() ? 1 : 0);
        for (const auto& b : models)
            equal.push_back(a == b ? 1 : 0);
    }
    writeNpy(out / "is_default.npy", isDefault);
    writeNpy(out / "equal.npy", {models.size(), models.size()}, equal.data());
}

void generateDistanceAttenuation(const std::filesystem::path& out)
{
    Lcg lcg(401);
    const auto d = distances(lcg);
    const auto models = distanceModels();

    std::vector<float> values;
    for (const auto& model : models)
        for (auto x : d)
            values.push_back(model.evaluate(x));
    writeNpy(out / "evaluate.npy", {models.size(), d.size()}, values.data());

    std::vector<float> curves;
    for (const auto& pair : kCurvePairs)
    {
        for (auto samplingRate : kSamplingRates)
        {
            std::vector<float> curve(kCurveLength);
            DistanceAttenuationModel::generateCorrectionCurve(models[pair[0]], models[pair[1]], samplingRate,
                                                              kCurveLength, curve.data());
            curves.insert(curves.end(), curve.begin(), curve.end());
        }
    }
    writeNpy(out / "correction_curves.npy",
             {std::size(kCurvePairs), std::size(kSamplingRates), static_cast<size_t>(kCurveLength)}, curves.data());

    writeIdentity(out, models);
}

// --------------------------------------------------------------------------------------------------------------------
// AirAbsorptionModel
// --------------------------------------------------------------------------------------------------------------------

float IPL_CALLBACK airCallback(float distance, int band, void* userData)
{
    const auto k = static_cast<const float*>(userData);
    return 1.0f / (1.0f + k[band] * distance);
}

float gAirK1[Bands::kNumBands] = {0.01f, 0.02f, 0.04f};
float gAirK2[Bands::kNumBands] = {0.1f, 0.2f, 0.3f};

void generateAirAbsorption(const std::filesystem::path& out)
{
    Lcg lcg(402);
    const auto d = distances(lcg);

    float random[Bands::kNumBands];
    for (auto& c : random)
        c = drawRange(lcg, 0.0f, 0.05f);

    const float defaults[Bands::kNumBands] = {0.0002f, 0.0017f, 0.0182f};
    const float zero[Bands::kNumBands] = {0.0f, 0.0f, 0.0f};
    const float negativeZero[Bands::kNumBands] = {-0.0f, 0.0f, 0.0f};
    const float negative[Bands::kNumBands] = {-0.01f, 0.0f, 0.5f};

    // Models in this order: default, the default coefficients given explicitly, random coefficients, zeros, a
    // negative zero, negative and large coefficients, a callback (twice), the callback with other user data.
    const std::vector<AirAbsorptionModel> models = {
        AirAbsorptionModel{},
        AirAbsorptionModel(defaults, nullptr, nullptr),
        AirAbsorptionModel(random, nullptr, nullptr),
        AirAbsorptionModel(zero, nullptr, nullptr),
        AirAbsorptionModel(negativeZero, nullptr, nullptr),
        AirAbsorptionModel(negative, nullptr, nullptr),
        AirAbsorptionModel(defaults, airCallback, gAirK1),
        AirAbsorptionModel(defaults, airCallback, gAirK1),
        AirAbsorptionModel(defaults, airCallback, gAirK2),
    };

    std::vector<float> values;
    for (const auto& model : models)
        for (auto x : d)
            for (auto band = 0; band < Bands::kNumBands; ++band)
                values.push_back(model.evaluate(x, band));
    writeNpy(out / "evaluate.npy", {models.size(), d.size(), static_cast<size_t>(Bands::kNumBands)}, values.data());

    writeIdentity(out, models);
}

// --------------------------------------------------------------------------------------------------------------------
// Directivity
// --------------------------------------------------------------------------------------------------------------------

float IPL_CALLBACK directivityCallback(Vector3f direction, void* userData)
{
    const auto k = *static_cast<const float*>(userData);
    return k + direction.x() + 2.0f * direction.y() + 3.0f * direction.z();
}

float gDirectivityK = 0.25f;

const float kFixedDipoles[][2] = {
    {0.0f, 0.0f}, {0.0f, 3.0f}, {0.5f, 1.0f}, {1.0f, 1.0f}, {1.0f, 2.0f},
    {0.3f, 0.5f}, {0.7f, 4.2f}, {-0.5f, 1.0f}, {1.5f, 2.0f}, {0.5f, 0.0f},
};
constexpr int kNumRandomDipoles = 20;
constexpr int kNumSpaces = 12;
constexpr int kNumPoints = 40;

void generateDirectivity(const std::filesystem::path& out)
{
    Lcg lcg(403);

    std::vector<Directivity> models;
    for (const auto& p : kFixedDipoles)
        models.emplace_back(p[0], p[1]);
    for (int i = 0; i < kNumRandomDipoles; ++i)
    {
        const float weight = drawRange(lcg, 0.0f, 1.0f);
        const float power = drawRange(lcg, 0.0f, 4.0f);
        models.emplace_back(weight, power);
    }
    models.emplace_back(0.0f, 0.0f, directivityCallback, &gDirectivityK);

    // The canonical space at the origin, then random unit ahead vectors at random origins.
    std::vector<CoordinateSpace3f> spaces;
    spaces.emplace_back();
    for (int i = 1; i < kNumSpaces; ++i)
    {
        const auto ahead = Vector3f::unitVector(drawVector(lcg));
        const auto origin = drawVector(lcg) * 10.0f;
        spaces.emplace_back(ahead, origin);
    }

    // Per space: its origin (a zero direction), then random points.
    std::vector<float> values;
    std::vector<Vector3f> points;
    for (const auto& space : spaces)
    {
        points.clear();
        points.push_back(space.origin);
        for (int i = 1; i < kNumPoints; ++i)
            points.push_back(drawVector(lcg) * 10.0f);

        for (const auto& model : models)
            for (const auto& point : points)
                values.push_back(model.evaluateAt(point, space));
    }
    writeNpy(out / "evaluate_at.npy",
             {static_cast<size_t>(kNumSpaces), models.size(), static_cast<size_t>(kNumPoints)}, values.data());
}

// --------------------------------------------------------------------------------------------------------------------
// GainEffect
// --------------------------------------------------------------------------------------------------------------------

// Operations: 'a' applies a new random gain, 'i' does the same in place, 'r' resets, 't' applies the previous gain
// (tailApply), 's' outputs the tail. Every operation except 'r' records a frame.
constexpr const char* kGainScript = "aaaiaaattaraaisa";
constexpr int kFrameSizes[] = {37, 100, 1};

void generateGain(const std::filesystem::path& out)
{
    Lcg lcg(404);

    for (auto frameSize : kFrameSizes)
    {
        AudioSettings settings{48000, frameSize};
        GainEffect effect(settings);
        AudioBuffer in(1, frameSize);
        AudioBuffer buffer(1, frameSize);

        std::vector<float> outputs;
        for (const char* op = kGainScript; *op; ++op)
        {
            switch (*op)
            {
            case 'a':
            case 'i': {
                const float gain = drawRange(lcg, 0.0f, 2.0f);
                fill(lcg, in);
                GainEffectParams params{};
                params.gain = gain;
                if (*op == 'a')
                {
                    effect.apply(params, in, buffer);
                }
                else
                {
                    for (int j = 0; j < frameSize; ++j)
                        buffer[0][j] = in[0][j];
                    effect.apply(params, buffer, buffer);
                }
                break;
            }
            case 't':
                fill(lcg, in);
                effect.tailApply(in, buffer);
                break;
            case 's':
                effect.tail(buffer);
                break;
            case 'r':
                effect.reset();
                continue;
            }
            append(outputs, buffer);
        }
        writeNpy(out / ("frame" + std::to_string(frameSize) + ".npy"), outputs);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// EQEffect
// --------------------------------------------------------------------------------------------------------------------

constexpr int kNumRandomNormalizations = 200;

void generateNormalizeGains(const std::filesystem::path& out, Lcg& lcg)
{
    const float denormal = std::numeric_limits<float>::denorm_min();
    const float tiny = std::numeric_limits<float>::min();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<std::array<float, 4>> cases = {
        {0.0f, 0.0f, 0.0f, 2.0f},
        {denormal, 0.0f, denormal, 2.0f},
        {0.0f, tiny, 0.0f, 3.0f},
        {-1.0f, -0.5f, -2.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 0.5f},
        {0.01f, 1.0f, 0.001f, 0.7f},
        {nan, 0.5f, 0.25f, 1.0f},
        {0.5f, nan, 0.25f, 1.0f},
        {4.0f, 2.0f, 0.1f, 0.25f},
    };
    for (int i = 0; i < kNumRandomNormalizations; ++i)
    {
        std::array<float, 4> c;
        for (auto& v : c)
            v = drawRange(lcg, 0.0f, 1.5f);
        cases.push_back(c);
    }

    std::vector<float> inputs;
    std::vector<float> outputs;
    for (auto c : cases)
    {
        inputs.insert(inputs.end(), c.begin(), c.end());
        float gains[Bands::kNumBands] = {c[0], c[1], c[2]};
        float overall = c[3];
        EQEffect::normalizeGains(gains, overall);
        outputs.insert(outputs.end(), std::begin(gains), std::end(gains));
        outputs.push_back(overall);
    }
    writeNpy(out / "normalize_inputs.npy", {cases.size(), 4}, inputs.data());
    writeNpy(out / "normalize_outputs.npy", {cases.size(), 4}, outputs.data());
}

// Operations: 'a' applies new random gains, 'k' applies the previous gains again (no crossfade), 'i' applies new
// gains in place, 'r' resets, 't' applies the previous gains (tailApply), 's' outputs the tail. Every operation
// except 'r' records a frame.
constexpr const char* kEqScript = "akaakiaktrakaiatsa";

struct EqCase
{
    int samplingRate;
    int frameSize;
};

constexpr EqCase kEqCases[] = {{48000, 37}, {44100, 64}, {24000, 100}, {48000, 5}};

void generateEq(const std::filesystem::path& out)
{
    Lcg lcg(405);
    generateNormalizeGains(out, lcg);

    for (const auto& c : kEqCases)
    {
        AudioSettings settings{c.samplingRate, c.frameSize};
        Context::sSIMDLevel = SIMDLevel::AVX;
        EQEffect effect(settings);
        Context::sSIMDLevel = SIMDLevel::SSE2;

        AudioBuffer in(1, c.frameSize);
        AudioBuffer buffer(1, c.frameSize);
        float gains[Bands::kNumBands] = {1.0f, 1.0f, 1.0f};

        std::vector<float> outputs;
        for (const char* op = kEqScript; *op; ++op)
        {
            switch (*op)
            {
            case 'a':
            case 'k':
            case 'i': {
                if (*op != 'k')
                {
                    for (auto& g : gains)
                        g = drawRange(lcg, 0.05f, 2.0f);
                }
                fill(lcg, in);
                EQEffectParams params{};
                params.gains = gains;
                if (*op == 'i')
                {
                    for (int j = 0; j < c.frameSize; ++j)
                        buffer[0][j] = in[0][j];
                    effect.apply(params, buffer, buffer);
                }
                else
                {
                    effect.apply(params, in, buffer);
                }
                break;
            }
            case 't':
                fill(lcg, in);
                effect.tailApply(in, buffer);
                break;
            case 's':
                effect.tail(buffer);
                break;
            case 'r':
                effect.reset();
                continue;
            }
            append(outputs, buffer);
        }
        writeNpy(out / ("run_" + std::to_string(c.samplingRate) + "_" + std::to_string(c.frameSize) + ".npy"),
                 outputs);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// DirectEffect
// --------------------------------------------------------------------------------------------------------------------

DirectSoundPath drawPath(Lcg& lcg)
{
    DirectSoundPath path{};
    path.distanceAttenuation = drawRange(lcg, 0.0f, 1.5f);
    for (auto& a : path.airAbsorption)
        a = drawRange(lcg, 0.0f, 1.0f);
    path.delay = drawRange(lcg, 0.0f, 1.0f);
    path.occlusion = drawRange(lcg, 0.0f, 1.0f);
    for (auto& t : path.transmission)
        t = drawRange(lcg, 0.0f, 1.0f);
    path.directivity = drawRange(lcg, 0.0f, 1.0f);
    return path;
}

// Per flag combination: frames with new random paths ('a'), the previous path again ('k'), a path without any air
// absorption or transmission ('z', which normalizeGains turns into an overall gain of 0), a reset ('r', no output) and
// the tail ('s').
constexpr const char* kDirectScript = "aakazarakas";
constexpr int kNumFlagCombinations = 32;

struct DirectCase
{
    int samplingRate;
    int frameSize;
    int numChannels;
};

constexpr DirectCase kDirectCases[] = {{48000, 37, 2}, {44100, 64, 1}, {24000, 20, 3}};

bool same(const AudioBuffer& a, const AudioBuffer& b)
{
    for (int i = 0; i < a.numChannels(); ++i)
    {
        if (memcmp(a[i], b[i], a.numSamples() * sizeof(float)) != 0)
            return false;
    }
    return true;
}

void generateDirect(const std::filesystem::path& out)
{
    Lcg lcg(406);

    for (const auto& c : kDirectCases)
    {
        AudioSettings settings{c.samplingRate, c.frameSize};
        DirectEffectSettings effectSettings{};
        effectSettings.numChannels = c.numChannels;

        std::vector<float> outputs;
        for (auto transmissionType : {TransmissionType::FreqIndependent, TransmissionType::FreqDependent})
        {
            for (int flags = 0; flags < kNumFlagCombinations; ++flags)
            {
                Context::sSIMDLevel = SIMDLevel::AVX;
                DirectEffect effect(settings, effectSettings);
                DirectEffect inPlace(settings, effectSettings);
                Context::sSIMDLevel = SIMDLevel::SSE2;

                AudioBuffer in(c.numChannels, c.frameSize);
                AudioBuffer buffer(c.numChannels, c.frameSize);
                AudioBuffer inPlaceBuffer(c.numChannels, c.frameSize);

                DirectEffectParams params{};
                params.flags = static_cast<DirectEffectFlags>(flags);
                params.transmissionType = transmissionType;

                for (const char* op = kDirectScript; *op; ++op)
                {
                    switch (*op)
                    {
                    case 'a':
                        params.directPath = drawPath(lcg);
                        break;
                    case 'z':
                        params.directPath = drawPath(lcg);
                        for (auto& a : params.directPath.airAbsorption)
                            a = 0.0f;
                        for (auto& t : params.directPath.transmission)
                            t = 0.0f;
                        params.directPath.occlusion = 0.0f;
                        break;
                    case 'r':
                        effect.reset();
                        inPlace.reset();
                        continue;
                    case 's':
                        effect.tail(buffer);
                        append(outputs, buffer);
                        continue;
                    }

                    fill(lcg, in);
                    effect.apply(params, in, buffer);

                    // phonon.h allows the direct effect to run in place; it must give the same result.
                    for (int i = 0; i < c.numChannels; ++i)
                        memcpy(inPlaceBuffer[i], in[i], c.frameSize * sizeof(float));
                    inPlace.apply(params, inPlaceBuffer, inPlaceBuffer);
                    if (!same(buffer, inPlaceBuffer))
                        throw std::runtime_error("DirectEffect in place differs from out of place");

                    append(outputs, buffer);
                }
            }
        }

        const auto name = std::to_string(c.samplingRate) + "_" + std::to_string(c.frameSize) + "_" +
                          std::to_string(c.numChannels);
        writeNpy(out / ("run_" + name + ".npy"), outputs);
    }
}

Register distanceAttenuation("distance_attenuation", generateDistanceAttenuation);
Register airAbsorption("air_absorption", generateAirAbsorption);
Register directivity("directivity", generateDirectivity);
Register gainEffect("gain_effect", generateGain);
Register eqEffect("eq_effect", generateEq);
Register directEffect("direct_effect", generateDirect);

}
