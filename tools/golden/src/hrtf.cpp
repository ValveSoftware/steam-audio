// Golden data for stage 3.1: hrtf_map.{h,cpp} with the built-in cipic_124 data, hrtf_database.{h,cpp}.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/hrtf.rs replays the
// same order. Spectra are stored as [n, bins, 2].

#include <cstring>
#include <string>

#include <hrtf_database.h>
#include <hrtf_map.h>
#include <sh.h>

#include "generators.h"
#include "npy.h"

extern ipl::byte_t gDefaultHrtfData[];

namespace {

using namespace golden;
using namespace ipl;

constexpr int kSamplingRates[] = {44100, 48000, 24000};
constexpr int kNumEars = IHRTFMap::kNumEars;
constexpr int kNumAmbisonicsCoeffs = 16;

// Axes, near-lateral directions on both sides of the median plane (z > 0 swaps the azimuth bounds), then `count`
// random ones.
std::vector<Vector3f> queryDirections(Lcg& lcg, int count)
{
    std::vector<Vector3f> directions = {
        {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.98f, 0.0f, 0.2f}, {0.98f, 0.1f, -0.2f},
        {-0.99f, 0.05f, 0.1f}, {-0.3f, 0.5f, 0.8f},
    };
    for (int i = 0; i < count; ++i)
    {
        const float x = lcg.nextSigned();
        const float y = lcg.nextSigned();
        const float z = lcg.nextSigned();
        directions.push_back(Vector3f(x, y, z));
    }
    return directions;
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
// HRTFMap
// --------------------------------------------------------------------------------------------------------------------

constexpr int kNumSelectedHRIRs = 6;
constexpr int kNumRandomMapDirections = 300;

void generateHrtfMap(const std::filesystem::path& out)
{
    Lcg lcg(31);

    for (int samplingRate : kSamplingRates)
    {
        HRTFMap map(samplingRate);
        const auto prefix = std::to_string(samplingRate) + "_";
        const int numSamples = map.numSamples();

        writeNpy(out / (prefix + "info.npy"), std::vector<int32_t>{map.numHRIRs(), numSamples});

        auto& hrirs = map.hrtfData();
        std::vector<int32_t> indices;
        std::vector<float> selected;
        for (int k = 0; k < kNumSelectedHRIRs; ++k)
        {
            const int index = static_cast<int>(lcg.nextU32() % static_cast<uint32_t>(map.numHRIRs()));
            indices.push_back(index);
            for (int ear = 0; ear < kNumEars; ++ear)
            {
                selected.insert(selected.end(), hrirs[ear][index], hrirs[ear][index] + numSamples);
            }
        }
        writeNpy(out / (prefix + "selected_indices.npy"), indices);
        writeNpy(out / (prefix + "selected_hrirs.npy"), {selected.size() / numSamples, size_t(numSamples)},
                 selected.data());

        auto& ambisonics = map.ambisonicsData();
        writeNpy(out / (prefix + "ambisonics_hrirs.npy"), {size_t(kNumEars * kNumAmbisonicsCoeffs), size_t(numSamples)},
                 ambisonics.flatData());
    }

    // Queries only depend on the measurement directions, which are the same at every sampling rate.
    HRTFMap map(44100);
    const auto directions = queryDirections(lcg, kNumRandomMapDirections);

    std::vector<int32_t> nearest;
    std::vector<int32_t> indices;
    std::vector<float> weights;
    for (const auto& direction : directions)
    {
        nearest.push_back(map.nearestHRIR(direction));

        int i[8];
        float w[8];
        map.interpolatedHRIRWeights(direction, i, w);
        indices.insert(indices.end(), i, i + 8);
        weights.insert(weights.end(), w, w + 8);
    }
    writeNpy(out / "nearest.npy", nearest);
    writeNpy(out / "interpolated_indices.npy", {directions.size(), 8}, indices.data());
    writeNpy(out / "interpolated_weights.npy", {directions.size(), 8}, weights.data());
}

// --------------------------------------------------------------------------------------------------------------------
// HRTFDatabase
// --------------------------------------------------------------------------------------------------------------------

struct DatabaseCase
{
    const char* name;
    int samplingRate;
    int frameSize;
    HRTFNormType normType;
    float volume;
    bool versionTwo; // Data without Ambisonics HRIRs, so the database projects them itself.
};

const DatabaseCase kDatabaseCases[] = {
    {"plain", 48000, 64, HRTFNormType::None, 0.0f, false},
    {"rms", 44100, 160, HRTFNormType::RMS, -3.0f, false},
    {"v2", 24000, 100, HRTFNormType::None, 4.5f, true},
};

constexpr int kNumSelectedHRTFs = 4;
constexpr int kNumRandomDatabaseDirections = 3;

struct BlendQuery
{
    float spatialBlend;
    HRTFPhaseType phaseType;
};

const BlendQuery kNearestQueries[] = {
    {0.3f, HRTFPhaseType::None},
    {0.6f, HRTFPhaseType::SphereITD},
    {0.5f, HRTFPhaseType::Full},
};

const BlendQuery kInterpolatedQueries[] = {
    {1.0f, HRTFPhaseType::None},
    {0.4f, HRTFPhaseType::SphereITD},
    {0.7f, HRTFPhaseType::None},
    {0.2f, HRTFPhaseType::Full},
};

// The built-in data up to the end of its HRIRs, relabelled as format version 2 (no Ambisonics HRIRs).
std::vector<uint8_t> versionTwoData()
{
    const auto readInt = [](size_t offset) {
        int32_t value;
        std::memcpy(&value, gDefaultHrtfData + offset, sizeof(value));
        return value;
    };

    const int numHRIRs = readInt(8);
    size_t offset = 12 + 8 * static_cast<size_t>(numHRIRs);
    const int numSamplingRates = readInt(offset);
    offset += 4;

    size_t hrirBytes = 0;
    for (int i = 0; i < numSamplingRates; ++i)
    {
        hrirBytes += 2 * static_cast<size_t>(numHRIRs) * readInt(offset + 8 * i + 4) * sizeof(float);
    }
    offset += 8 * static_cast<size_t>(numSamplingRates);

    std::vector<uint8_t> data(gDefaultHrtfData, gDefaultHrtfData + offset + hrirBytes);
    const int32_t version = 2;
    std::memcpy(data.data() + 4, &version, sizeof(version));
    return data;
}

void generateDatabaseCase(const std::filesystem::path& out, const DatabaseCase& c, Lcg& lcg)
{
    const auto v2 = versionTwoData();

    HRTFSettings settings{};
    settings.type = HRTFMapType::Default;
    settings.hrtfData = c.versionTwo ? v2.data() : nullptr;
    settings.volume = c.volume;
    settings.normType = c.normType;

    HRTFDatabase database(settings, c.samplingRate, c.frameSize);
    const auto prefix = std::string(c.name) + "_";
    const int bins = database.numSpectrumSamples();

    writeNpy(out / (prefix + "info.npy"),
             std::vector<int32_t>{database.numHRIRs(), database.numSamples(), bins});

    std::vector<int32_t> indices;
    std::vector<complex_t> selected;
    std::vector<float> loudness;
    for (int k = 0; k < kNumSelectedHRTFs; ++k)
    {
        const int index = static_cast<int>(lcg.nextU32() % static_cast<uint32_t>(database.numHRIRs()));
        indices.push_back(index);

        const complex_t* hrtf[kNumEars] = {};
        database.getHRTFByIndex(index, hrtf);
        for (int ear = 0; ear < kNumEars; ++ear)
        {
            append(selected, hrtf[ear], bins);
        }
        loudness.push_back(Loudness::calculateRMSLoudness(bins, c.samplingRate, hrtf));
    }
    writeNpy(out / (prefix + "selected_indices.npy"), indices);
    writeSpectra(out / (prefix + "selected_hrtfs.npy"), selected, bins);
    writeNpy(out / (prefix + "selected_loudness.npy"), loudness);

    std::vector<complex_t> ambisonics;
    for (int k = 0; k < kNumAmbisonicsCoeffs; ++k)
    {
        const complex_t* hrtf[kNumEars] = {};
        database.ambisonicsHRTF(k, hrtf);
        for (int ear = 0; ear < kNumEars; ++ear)
        {
            append(ambisonics, hrtf[ear], bins);
        }
    }
    writeSpectra(out / (prefix + "ambisonics_hrtfs.npy"), ambisonics, bins);

    const auto directions = queryDirections(lcg, kNumRandomDatabaseDirections);

    Array<complex_t, 2> blended(kNumEars, bins);
    std::vector<int32_t> nearestPeaks;
    std::vector<complex_t> nearestBlended;
    std::vector<int32_t> interpolatedPeaks;
    std::vector<complex_t> interpolated;
    for (const auto& direction : directions)
    {
        for (const auto& q : kNearestQueries)
        {
            const complex_t* hrtf[kNumEars] = {};
            int peaks[kNumEars] = {};
            database.nearestHRTF(direction, hrtf, q.spatialBlend, q.phaseType, blended.data(), peaks);
            nearestPeaks.insert(nearestPeaks.end(), peaks, peaks + kNumEars);
            for (int ear = 0; ear < kNumEars; ++ear)
            {
                append(nearestBlended, blended[ear], bins);
            }
        }

        for (const auto& q : kInterpolatedQueries)
        {
            int peaks[kNumEars] = {};
            database.interpolatedHRTF(direction, blended.data(), q.spatialBlend, q.phaseType, peaks);
            interpolatedPeaks.insert(interpolatedPeaks.end(), peaks, peaks + kNumEars);
            for (int ear = 0; ear < kNumEars; ++ear)
            {
                append(interpolated, blended[ear], bins);
            }
        }
    }
    writeNpy(out / (prefix + "nearest_peaks.npy"), nearestPeaks);
    writeSpectra(out / (prefix + "nearest_blended.npy"), nearestBlended, bins);
    writeNpy(out / (prefix + "interpolated_peaks.npy"), interpolatedPeaks);
    writeSpectra(out / (prefix + "interpolated.npy"), interpolated, bins);
}

void generateHrtfDatabase(const std::filesystem::path& out)
{
    Lcg lcg(37);
    for (const auto& c : kDatabaseCases)
    {
        generateDatabaseCase(out, c, lcg);
    }

    const std::vector<float> gains = {0.0f, 1e-5f, 3.2e-5f, 3.3e-5f, 0.1f, 0.5f, 1.0f, 2.0f, 10.0f};
    const std::vector<float> levels = {-120.0f, -90.0f, -89.9f, -40.0f, -6.0f, 0.0f, 3.0f, 12.0f, 20.0f};
    std::vector<float> gainToDB;
    std::vector<float> dBToGain;
    for (float g : gains)
    {
        gainToDB.push_back(Loudness::gainToDB(g));
    }
    for (float l : levels)
    {
        dBToGain.push_back(Loudness::dBToGain(l));
    }
    writeNpy(out / "gain_to_db.npy", gainToDB);
    writeNpy(out / "db_to_gain.npy", dBToGain);
}

Register hrtfMap("hrtf_map", generateHrtfMap);
Register hrtfDatabase("hrtf_database", generateHrtfDatabase);

}
