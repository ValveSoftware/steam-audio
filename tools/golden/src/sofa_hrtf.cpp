// Golden data for stage 3.5c: sofa_hrtf_map.{h,cpp} and hrtf_map_factory.{h,cpp}, then HRTFDatabase and
// BinauralEffect with a SOFA HRTF.
//
// Modules:
// - sofa_hrtf_map: for each file in core/data/hrtf, loaded through HRTFMapFactory at 48 kHz: sizes ("_info"),
//   selected HRIRs, the neighbourhood rows patchSOFANeighborhood fills in ("_patched": [index, +phi, -phi] for every
//   measurement libmysofa left without an azimuth neighbour), the same for Lcg-chosen measurements whose azimuth
//   neighbours are removed ("_repatched", [index, +phi, -phi]), and nearestHRIR / interpolatedHRIRWeights for fixed,
//   measured, near-measured and random directions.
// - sofa_hrtf_database: HRTFDatabase cases with a SOFA HRTF (selected HRTFs, projected Ambisonics HRTFs, nearest and
//   interpolated queries), and BinauralEffect scripts as in spatialization.cpp.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/sofa_hrtf.rs replays
// the same order. Spectra are stored as [n, bins, 2]. Run from the repository root (paths are relative to it).

#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#include <mysofa.h>

#include <audio_buffer.h>
#include <binaural_effect.h>
#include <hrtf_database.h>
#include <hrtf_map.h>
#include <hrtf_map_factory.h>
#include <sofa_hrtf_map.h>

#include "generators.h"
#include "npy.h"

// SOFAHRTFMap keeps its MYSOFA_EASY (source positions, patched neighbourhood) and patchSOFANeighborhood private. Access
// checks do not apply to the template arguments of an explicit instantiation, so these helpers name them without
// changing the class.
namespace {

using SofaMember = MYSOFA_EASY* ipl::SOFAHRTFMap::*;
using PatchMember = void (ipl::SOFAHRTFMap::*)();

SofaMember sofaMember();
PatchMember patchMember();

template <SofaMember M>
struct SofaAccess
{
    friend SofaMember sofaMember() { return M; }
};

template <PatchMember M>
struct PatchAccess
{
    friend PatchMember patchMember() { return M; }
};

template struct SofaAccess<&ipl::SOFAHRTFMap::mSOFA>;
template struct PatchAccess<&ipl::SOFAHRTFMap::patchSOFANeighborhood>;

MYSOFA_EASY* sofaOf(ipl::SOFAHRTFMap& map)
{
    return map.*sofaMember();
}

}

namespace {

using namespace golden;
using namespace ipl;

constexpr const char* kFiles[] = {"cipic_124", "sadie_d1", "sadie_h12"};
constexpr int kMapRate = 48000;
constexpr int kNumEars = IHRTFMap::kNumEars;
constexpr int kNumAmbisonicsCoeffs = 16;

std::filesystem::path sofaPath(const std::string& name)
{
    return std::filesystem::path("core/data/hrtf") / (name + ".sofa");
}

std::vector<char> readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open " + path.string());
    return std::vector<char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

HRTFSettings sofaDataSettings(const std::vector<char>& data)
{
    HRTFSettings settings{};
    settings.type = HRTFMapType::SOFA;
    settings.sofaData = reinterpret_cast<const uint8_t*>(data.data());
    settings.sofaDataSize = static_cast<int>(data.size());
    return settings;
}

Vector3f drawVector(Lcg& lcg)
{
    const float x = lcg.nextSigned();
    const float y = lcg.nextSigned();
    const float z = lcg.nextSigned();
    return Vector3f(x, y, z);
}

// The Steam Audio direction that toSOFACoordinates maps onto the SOFA position p.
Vector3f fromSOFA(const float* p)
{
    return Vector3f(-p[1], p[2], -p[0]);
}

void writeSpectra(const std::filesystem::path& path, const std::vector<complex_t>& data, size_t bins)
{
    writeNpy(path, {data.size() / bins, bins, 2}, reinterpret_cast<const float*>(data.data()));
}

void append(std::vector<complex_t>& to, const complex_t* from, int size)
{
    to.insert(to.end(), from, from + size);
}

// --------------------------------------------------------------------------------------------------------------------
// SOFAHRTFMap
// --------------------------------------------------------------------------------------------------------------------

constexpr int kNumSelectedHRIRs = 6;
constexpr int kNumMeasuredDirections = 20;
constexpr int kNumNearDirections = 40;
constexpr int kNumRandomDirections = 200;
constexpr int kNumRepatched = 40;

void generateMap(const std::filesystem::path& out)
{
    std::filesystem::create_directories(out);
    Lcg lcg(73);

    for (uint32_t fileIndex = 0; fileIndex < std::size(kFiles); ++fileIndex)
    {
        const char* name = kFiles[fileIndex];
        const auto data = readFile(sofaPath(name));
        auto map = HRTFMapFactory::create(sofaDataSettings(data), kMapRate);
        auto& sofaMap = dynamic_cast<SOFAHRTFMap&>(*map);
        const auto prefix = std::string(name) + "_";
        const int numHRIRs = map->numHRIRs();
        const int numSamples = map->numSamples();

        writeNpy(out / (prefix + "info.npy"),
                 std::vector<int32_t>{numHRIRs, numSamples, static_cast<int32_t>(map->ambisonicsData().totalSize())});

        auto& hrirs = map->hrtfData();
        std::vector<int32_t> indices;
        std::vector<float> selected;
        for (int k = 0; k < kNumSelectedHRIRs; ++k)
        {
            const int index = static_cast<int>(lcg.nextU32() % static_cast<uint32_t>(numHRIRs));
            indices.push_back(index);
            for (int ear = 0; ear < kNumEars; ++ear)
                selected.insert(selected.end(), hrirs[ear][index], hrirs[ear][index] + numSamples);
        }
        writeNpy(out / (prefix + "selected_indices.npy"), indices);
        writeNpy(out / (prefix + "selected_hrirs.npy"), {selected.size() / numSamples, size_t(numSamples)},
                 selected.data());

        // The neighbourhood before patching, from a second, unpatched load.
        int err = 0;
        int filterLength = 0;
        auto* unpatched = mysofa_open_data_no_norm(data.data(), static_cast<long>(data.size()),
                                                   static_cast<float>(kMapRate), &filterLength, &err);
        if (!unpatched)
            throw std::runtime_error(std::string("cannot open ") + name);
        const int* before = unpatched->neighborhood->index;
        const int* after = sofaOf(sofaMap)->neighborhood->index;
        std::vector<int32_t> patched;
        for (int i = 0; i < numHRIRs; ++i)
        {
            for (int k = 2; k < 6; ++k)
            {
                if (before[6 * i + k] != after[6 * i + k])
                    throw std::runtime_error("patchSOFANeighborhood changed an elevation or radius neighbour");
            }
            if (before[6 * i] < 0 || before[6 * i + 1] < 0)
                patched.insert(patched.end(), {i, after[6 * i], after[6 * i + 1]});
        }
        mysofa_close(unpatched);
        writeNpy(out / (prefix + "patched.npy"), {patched.size() / 3, 3}, patched.data());


        // Fixed directions (the zero vector last), measured positions (interpolation is skipped), positions moved by
        // up to 0.05 per coordinate, and random directions.
        const float* positions = sofaOf(sofaMap)->hrtf->SourcePosition.values;
        std::vector<Vector3f> directions = {
            {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.98f, 0.0f, 0.2f}, {0.98f, 0.1f, -0.2f},
            {-0.99f, 0.05f, 0.1f}, {-0.3f, 0.5f, 0.8f}, {0.0f, 0.0f, 0.0f},
        };
        for (int k = 0; k < kNumMeasuredDirections; ++k)
        {
            const uint32_t index = lcg.nextU32() % static_cast<uint32_t>(numHRIRs);
            directions.push_back(fromSOFA(positions + 3 * index));
        }
        for (int k = 0; k < kNumNearDirections; ++k)
        {
            const uint32_t index = lcg.nextU32() % static_cast<uint32_t>(numHRIRs);
            const float dx = lcg.nextSigned() * 0.05f;
            const float dy = lcg.nextSigned() * 0.05f;
            const float dz = lcg.nextSigned() * 0.05f;
            directions.push_back(fromSOFA(positions + 3 * index) + Vector3f(dx, dy, dz));
        }
        for (int k = 0; k < kNumRandomDirections; ++k)
            directions.push_back(drawVector(lcg));

        std::vector<int32_t> nearest;
        std::vector<int32_t> interpolatedIndices;
        std::vector<float> interpolatedWeights;
        for (const auto& direction : directions)
        {
            nearest.push_back(map->nearestHRIR(direction));

            // Initialized as HRTFDatabase::interpolatedHRTF does: a direction at a measurement only sets slot 0.
            int i[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            float w[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
            map->interpolatedHRIRWeights(direction, i, w);
            interpolatedIndices.insert(interpolatedIndices.end(), i, i + 8);
            interpolatedWeights.insert(interpolatedWeights.end(), w, w + 8);
        }
        writeNpy(out / (prefix + "nearest.npy"), nearest);
        writeNpy(out / (prefix + "interpolated_indices.npy"), {directions.size(), 8}, interpolatedIndices.data());
        writeNpy(out / (prefix + "interpolated_weights.npy"), {directions.size(), 8}, interpolatedWeights.data());

        // The shipped files only lack azimuth neighbours at the poles, where none is found. Remove the +phi, the -phi
        // or both neighbours of measurements chosen by a separate Lcg, and patch again: [index, +phi, -phi] per
        // chosen measurement (checked by the unit tests in crates/steam-audio/src/hrtf/sofa_map.rs).
        Lcg repatchLcg(83 + fileIndex);
        int* table = sofaOf(sofaMap)->neighborhood->index;
        std::vector<int32_t> chosen;
        for (int k = 0; k < kNumRepatched; ++k)
        {
            const int index = static_cast<int>(repatchLcg.nextU32() % static_cast<uint32_t>(numHRIRs));
            const uint32_t sides = repatchLcg.nextU32() % 3;
            if (sides != 1)
                table[6 * index] = -1;
            if (sides != 0)
                table[6 * index + 1] = -1;
            chosen.push_back(index);
        }
        (sofaMap.*patchMember())();
        std::vector<int32_t> repatched;
        for (int index : chosen)
            repatched.insert(repatched.end(), {index, table[6 * index], table[6 * index + 1]});
        writeNpy(out / (prefix + "repatched.npy"), {repatched.size() / 3, 3}, repatched.data());
    }
}

// --------------------------------------------------------------------------------------------------------------------
// HRTFDatabase and BinauralEffect
// --------------------------------------------------------------------------------------------------------------------

struct DatabaseCase
{
    const char* name;
    const char* file;
    bool fromFileName; // sofaFileName instead of sofaData.
    int samplingRate;
    int frameSize;
    HRTFNormType normType;
    float volume;
};

const DatabaseCase kDatabaseCases[] = {
    {"cipic", "cipic_124", true, 48000, 64, HRTFNormType::None, 0.0f},
    {"h12_rms", "sadie_h12", false, 44100, 100, HRTFNormType::RMS, -3.0f},
};

constexpr int kNumSelectedHRTFs = 3;
constexpr int kNumRandomDatabaseDirections = 3;

struct BlendQuery
{
    float spatialBlend;
    HRTFPhaseType phaseType;
};

const BlendQuery kInterpolatedQueries[] = {
    {1.0f, HRTFPhaseType::None},
    {0.4f, HRTFPhaseType::SphereITD},
};

std::unique_ptr<HRTFDatabase> makeDatabase(const DatabaseCase& c, const std::vector<char>& data,
                                           const std::string& fileName)
{
    auto settings = sofaDataSettings(data);
    if (c.fromFileName)
    {
        settings.sofaData = nullptr;
        settings.sofaDataSize = 0;
        settings.sofaFileName = fileName.c_str();
    }
    settings.volume = c.volume;
    settings.normType = c.normType;
    return std::make_unique<HRTFDatabase>(settings, c.samplingRate, c.frameSize);
}

void generateDatabaseCase(const std::filesystem::path& out, const DatabaseCase& c, Lcg& lcg)
{
    const auto fileName = sofaPath(c.file).string();
    const auto data = readFile(fileName);
    auto database = makeDatabase(c, data, fileName);
    const auto prefix = std::string(c.name) + "_";
    const int bins = database->numSpectrumSamples();

    writeNpy(out / (prefix + "info.npy"),
             std::vector<int32_t>{database->numHRIRs(), database->numSamples(), bins});

    std::vector<int32_t> indices;
    std::vector<complex_t> selected;
    for (int k = 0; k < kNumSelectedHRTFs; ++k)
    {
        const int index = static_cast<int>(lcg.nextU32() % static_cast<uint32_t>(database->numHRIRs()));
        indices.push_back(index);
        const complex_t* hrtf[kNumEars] = {};
        database->getHRTFByIndex(index, hrtf);
        for (int ear = 0; ear < kNumEars; ++ear)
            append(selected, hrtf[ear], bins);
    }
    writeNpy(out / (prefix + "selected_indices.npy"), indices);
    writeSpectra(out / (prefix + "selected_hrtfs.npy"), selected, bins);

    std::vector<complex_t> ambisonics;
    for (int k = 0; k < kNumAmbisonicsCoeffs; ++k)
    {
        const complex_t* hrtf[kNumEars] = {};
        database->ambisonicsHRTF(k, hrtf);
        for (int ear = 0; ear < kNumEars; ++ear)
            append(ambisonics, hrtf[ear], bins);
    }
    writeSpectra(out / (prefix + "ambisonics_hrtfs.npy"), ambisonics, bins);

    std::vector<Vector3f> directions = {
        {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {-0.3f, 0.5f, 0.8f}, {0.98f, 0.1f, -0.2f},
    };
    for (int k = 0; k < kNumRandomDatabaseDirections; ++k)
        directions.push_back(drawVector(lcg));

    Array<complex_t, 2> blended(kNumEars, bins);
    std::vector<int32_t> nearestPeaks;
    std::vector<int32_t> interpolatedPeaks;
    std::vector<complex_t> interpolated;
    for (const auto& direction : directions)
    {
        const complex_t* hrtf[kNumEars] = {};
        int peaks[kNumEars] = {};
        database->nearestHRTF(direction, hrtf, 1.0f, HRTFPhaseType::None, nullptr, peaks);
        nearestPeaks.insert(nearestPeaks.end(), peaks, peaks + kNumEars);

        for (const auto& q : kInterpolatedQueries)
        {
            database->interpolatedHRTF(direction, blended.data(), q.spatialBlend, q.phaseType, peaks);
            interpolatedPeaks.insert(interpolatedPeaks.end(), peaks, peaks + kNumEars);
            for (int ear = 0; ear < kNumEars; ++ear)
                append(interpolated, blended[ear], bins);
        }
    }
    writeNpy(out / (prefix + "nearest_peaks.npy"), nearestPeaks);
    writeNpy(out / (prefix + "interpolated_peaks.npy"), interpolatedPeaks);
    writeSpectra(out / (prefix + "interpolated.npy"), interpolated, bins);
}

// A: apply with a random direction; T: tail; R: reset. As spatialization.cpp, with the cipic_124 SOFA file at 48 kHz.
struct BinauralCase
{
    const char* name;
    int inChannels;
    HRTFInterpolation interpolation;
    float spatialBlend;
    HRTFPhaseType phaseType;
    const char* script;
};

constexpr int kBinauralFrameSize = 64;

constexpr BinauralCase kBinauralCases[] = {
    {"binaural_nearest_mono", 1, HRTFInterpolation::NearestNeighbor, 1.0f, HRTFPhaseType::None, "AAAATTRAA"},
    {"binaural_bilinear_mono", 1, HRTFInterpolation::Bilinear, 1.0f, HRTFPhaseType::None, "AAAATTTTAA"},
    {"binaural_bilinear_stereo_blend_itd", 2, HRTFInterpolation::Bilinear, 0.6f, HRTFPhaseType::SphereITD, "AAATTA"},
};

void fill(Lcg& lcg, AudioBuffer& buffer)
{
    for (int i = 0; i < buffer.numChannels(); ++i)
    {
        for (int j = 0; j < buffer.numSamples(); ++j)
            buffer[i][j] = lcg.nextSigned();
    }
}

void generateBinaural(const std::filesystem::path& out, Lcg& lcg)
{
    const auto data = readFile(sofaPath("cipic_124"));
    HRTFDatabase database(sofaDataSettings(data), kMapRate, kBinauralFrameSize);

    AudioSettings audioSettings{};
    audioSettings.samplingRate = kMapRate;
    audioSettings.frameSize = kBinauralFrameSize;

    for (const auto& c : kBinauralCases)
    {
        BinauralEffectSettings settings{};
        settings.hrtf = &database;
        BinauralEffect effect(audioSettings, settings);

        AudioBuffer in(c.inChannels, kBinauralFrameSize);
        AudioBuffer output(2, kBinauralFrameSize);
        std::vector<float> outputs;
        std::vector<int32_t> states;
        std::vector<float> peakDelays;
        size_t count = 0;
        const auto record = [&](AudioEffectState state) {
            for (int i = 0; i < output.numChannels(); ++i)
                outputs.insert(outputs.end(), output[i], output[i] + output.numSamples());
            states.push_back(state == AudioEffectState::TailRemaining ? 0 : 1);
            states.push_back(effect.numTailSamplesRemaining());
            ++count;
        };

        for (const char* op = c.script; *op; ++op)
        {
            switch (*op)
            {
            case 'A':
            {
                auto direction = Vector3f::unitVector(drawVector(lcg));
                fill(lcg, in);
                float delays[2] = {};
                BinauralEffectParams params{};
                params.direction = &direction;
                params.interpolation = c.interpolation;
                params.spatialBlend = c.spatialBlend;
                params.phaseType = c.phaseType;
                params.hrtf = &database;
                params.peakDelays = delays;
                record(effect.apply(params, in, output));
                peakDelays.push_back(delays[0]);
                peakDelays.push_back(delays[1]);
                break;
            }
            case 'T':
                record(effect.tail(output));
                break;
            case 'R':
                effect.reset();
                break;
            }
        }

        writeNpy(out / (std::string(c.name) + ".npy"), {count, 2, static_cast<size_t>(kBinauralFrameSize)},
                 outputs.data());
        writeNpy(out / (std::string(c.name) + "_states.npy"), {count, 2}, states.data());
        writeNpy(out / (std::string(c.name) + "_peak_delays.npy"), {peakDelays.size() / 2, 2}, peakDelays.data());
    }
}

void generateDatabase(const std::filesystem::path& out)
{
    std::filesystem::create_directories(out);
    Lcg lcg(79);
    for (const auto& c : kDatabaseCases)
        generateDatabaseCase(out, c, lcg);
    generateBinaural(out, lcg);
}

Register sofaHrtfMap("sofa_hrtf_map", generateMap);
Register sofaHrtfDatabase("sofa_hrtf_database", generateDatabase);

}
