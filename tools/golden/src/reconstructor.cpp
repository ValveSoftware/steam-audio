// Golden data for stage 6.1: energy_field.{h,cpp}, impulse_response.{h,cpp}, reconstructor.{h,cpp}.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/reconstructor.rs
// replays the same order. Reconstructor filters its bands with IIRFilterer, which picks its block algorithm from
// Context::sSIMDLevel when it is constructed: reconstructors are built with the AVX level, the path the port follows
// (risk R8; needs an AVX CPU).
//
//   sizes:                 [channels, bins] of energy fields, then [channels, samples] of IRs, for kFieldSizes and
//                          kIRSizes;
//   energy_field_ops,
//   impulse_response_ops:  add, scale, scaleAccumulate and copy between objects of different sizes, each result
//                          appended whole;
//   reconstruct_<rate>:    every IR (whole) after each reconstruction in kCalls, from one Reconstructor per rate.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <air_absorption.h>
#include <context.h>
#include <energy_field.h>
#include <impulse_response.h>
#include <reconstructor.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

int totalSize(const EnergyField& f)
{
    return f.numChannels() * Bands::kNumBands * f.numBins();
}

int totalSize(const ImpulseResponse& ir)
{
    return ir.numChannels() * ir.numSamples();
}

template <typename T>
void fill(Lcg& lcg, T& x, float* data)
{
    for (auto i = 0; i < totalSize(x); ++i)
        data[i] = lcg.nextSigned();
}

void fill(Lcg& lcg, EnergyField& f)
{
    fill(lcg, f, f.flatData());
}

void fill(Lcg& lcg, ImpulseResponse& ir)
{
    fill(lcg, ir, ir.data());
}

void append(std::vector<float>& out, const EnergyField& f)
{
    out.insert(out.end(), f.flatData(), f.flatData() + totalSize(f));
}

void append(std::vector<float>& out, const ImpulseResponse& ir)
{
    out.insert(out.end(), ir.data(), ir.data() + totalSize(ir));
}

// ---------------------------------------------------------------------------------------------------------------------
// Sizes and operations
// ---------------------------------------------------------------------------------------------------------------------

struct FieldSize
{
    float duration;
    int order;
};

struct IRSize
{
    float duration;
    int order;
    int samplingRate;
};

const FieldSize kFieldSizes[] = {{0.0f, 0}, {0.005f, 0}, {0.01f, 1}, {0.0123f, 2}, {0.1f, 1}, {0.999f, 3}, {2.5f, 0}};
const IRSize kIRSizes[] = {{0.0f, 0, 48000},    {0.001f, 1, 48000}, {0.00625f, 1, 48000}, {0.1f, 2, 44100},
                           {0.0731f, 3, 22050}, {1.0f, 0, 24000},   {0.0537f, 1, 48000}};

void generateSizes(const std::filesystem::path& out)
{
    std::vector<int32_t> sizes;
    for (const auto& s : kFieldSizes)
    {
        EnergyField f(s.duration, s.order);
        sizes.push_back(f.numChannels());
        sizes.push_back(f.numBins());
    }
    for (const auto& s : kIRSizes)
    {
        ImpulseResponse ir(s.duration, s.order, s.samplingRate);
        sizes.push_back(ir.numChannels());
        sizes.push_back(ir.numSamples());
    }
    writeNpy(out / "sizes.npy", sizes);
}

// The same sequence for both types: a, b, c of different channel counts and lengths, drawn from the Lcg.
template <typename T, typename Copy>
std::vector<float> operations(Lcg& lcg, T& a, T& b, T& c, Copy copy)
{
    fill(lcg, a);
    fill(lcg, b);
    fill(lcg, c);

    std::vector<float> rows;
    T::add(a, b, c);
    append(rows, c);

    const float s1 = lcg.nextSigned();
    T::scale(b, s1, c);
    append(rows, c);

    const float s2 = lcg.nextSigned();
    T::scaleAccumulate(a, s2, c);
    append(rows, c);

    const float s3 = lcg.nextSigned();
    T::scaleAccumulate(c, s3, b);
    append(rows, b);

    copy(b, a);
    append(rows, a);

    copy(c, b);
    append(rows, b);

    a.reset();
    append(rows, a);
    return rows;
}

void generateOperations(const std::filesystem::path& out)
{
    Lcg lcg(601);

    EnergyField fa(0.05f, 1);
    EnergyField fb(0.03f, 2);
    EnergyField fc(0.07f, 0);
    writeNpy(out / "energy_field_ops.npy",
             operations(lcg, fa, fb, fc, [](const EnergyField& src, EnergyField& dst) { dst.copyFrom(src); }));

    ImpulseResponse ia(0.0011f, 1, 48000);
    ImpulseResponse ib(0.0007f, 2, 44100);
    ImpulseResponse ic(0.0013f, 0, 22050);
    writeNpy(out / "impulse_response_ops.npy",
             operations(lcg, ia, ib, ic,
                        [](const ImpulseResponse& src, ImpulseResponse& dst) { ImpulseResponse::copy(src, dst); }));
}

// ---------------------------------------------------------------------------------------------------------------------
// Reconstruction
// ---------------------------------------------------------------------------------------------------------------------

// Durations are fractions of the rate's maximum duration.
struct IRSpec
{
    float fieldDuration;
    int fieldOrder;
    float irDuration;
    int irOrder;
    bool correction;
    int airModel;
};

struct Call
{
    ReconstructionType type;
    float duration;
    int order;
    std::vector<IRSpec> irs;
};

const Call kCalls[] = {
    // Everything the same size.
    {ReconstructionType::Linear, 1.0f, 1, {{1.0f, 1, 1.0f, 1, false, 0}}},
    // A second IR whose field is shorter, so samples past its last bin keep the first IR's band noise.
    {ReconstructionType::Gaussian, 1.0f, 2, {{1.0f, 2, 1.0f, 2, true, 1}, {0.5f, 1, 1.0f, 1, false, 0}}},
    // A shorter duration and lower order than the IR holds.
    {ReconstructionType::Gaussian, 0.6f, 0, {{1.0f, 2, 1.0f, 1, false, 0}}},
    // An IR shorter than the duration and with fewer channels than the field; a field shorter than one bin's worth.
    {ReconstructionType::Linear, 1.0f, 2, {{1.0f, 2, 0.7f, 1, true, 1}, {0.35f, 0, 1.0f, 0, false, 1}}},
    {ReconstructionType::Gaussian,
     1.0f,
     1,
     {{1.0f, 1, 1.0f, 1, true, 0}, {0.8f, 1, 0.9f, 1, false, 1}, {1.0f, 0, 1.0f, 1, false, 0}}},
    {ReconstructionType::Linear, 0.25f, 1, {{1.0f, 1, 1.0f, 1, false, 0}}},
};

struct RateCase
{
    int samplingRate;
    float maxDuration;
    uint32_t seed;
};

const RateCase kRateCases[] = {{48000, 0.0337f, 611}, {44100, 0.0461f, 612}, {22050, 0.0593f, 613}};

// Channel 0 positive, other channels signed; about 20 % of the values silent and 5 % just below the threshold.
void fillEnergies(Lcg& lcg, EnergyField& f)
{
    for (auto i = 0; i < f.numChannels(); ++i)
    {
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            for (auto k = 0; k < f.numBins(); ++k)
            {
                const float u = lcg.nextSigned();
                const float z = lcg.nextSigned();
                float e = 0.0f;
                if (z < -0.6f)
                    e = 0.0f;
                else if (z < -0.5f)
                    e = 5e-8f;
                else if (i == 0)
                    e = 0.01f * (u + 1.0f);
                else
                    e = 0.01f * u;
                f[i][j][k] = e;
            }
        }
    }
}

void generateReconstruct(const std::filesystem::path& out)
{
    const float customCoefficients[Bands::kNumBands] = {0.05f, 0.2f, 0.9f};
    const AirAbsorptionModel airModels[] = {AirAbsorptionModel{},
                                            AirAbsorptionModel(customCoefficients, nullptr, nullptr)};

    for (const auto& rc : kRateCases)
    {
        Lcg lcg(rc.seed);

        Context::sSIMDLevel = SIMDLevel::AVX;
        Reconstructor reconstructor(rc.maxDuration, 2, rc.samplingRate);
        Context::sSIMDLevel = SIMDLevel::SSE2;

        std::vector<float> rows;
        for (const auto& call : kCalls)
        {
            std::vector<std::unique_ptr<EnergyField>> fields;
            std::vector<std::unique_ptr<ImpulseResponse>> irs;
            std::vector<std::vector<float>> curves;
            std::vector<const EnergyField*> fieldPointers;
            std::vector<ImpulseResponse*> irPointers;
            std::vector<const float*> curvePointers;
            std::vector<AirAbsorptionModel> models;

            for (const auto& spec : call.irs)
            {
                fields.push_back(std::make_unique<EnergyField>(rc.maxDuration * spec.fieldDuration, spec.fieldOrder));
                fillEnergies(lcg, *fields.back());

                irs.push_back(std::make_unique<ImpulseResponse>(rc.maxDuration * spec.irDuration, spec.irOrder,
                                                                rc.samplingRate));
                // Garbage the reconstruction must overwrite.
                fill(lcg, *irs.back());

                curves.emplace_back();
                if (spec.correction)
                {
                    for (auto i = 0; i < irs.back()->numSamples(); ++i)
                    {
                        const float u = lcg.nextSigned();
                        curves.back().push_back(1.0f + 0.5f * u);
                    }
                }

                fieldPointers.push_back(fields.back().get());
                irPointers.push_back(irs.back().get());
                models.push_back(airModels[spec.airModel]);
            }
            for (const auto& curve : curves)
                curvePointers.push_back(curve.empty() ? nullptr : curve.data());

            reconstructor.reconstruct(static_cast<int>(call.irs.size()), fieldPointers.data(), curvePointers.data(),
                                      models.data(), irPointers.data(), call.type, rc.maxDuration * call.duration,
                                      call.order);

            for (const auto& ir : irs)
                append(rows, *ir);
        }

        writeNpy(out / ("reconstruct_" + std::to_string(rc.samplingRate) + ".npy"), rows);
    }
}

void generateReconstructor(const std::filesystem::path& out)
{
    generateSizes(out);
    generateOperations(out);
    generateReconstruct(out);
}

Register reconstructor("reconstructor", generateReconstructor);

}
