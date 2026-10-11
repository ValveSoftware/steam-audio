// Golden data for stage 3.5b: the rest of libmysofa's mysofa_open_data_no_norm (mysofa_resample with the Speex
// resampler, mysofa_tocartesian, mysofa_lookup_init/mysofa_lookup, mysofa_neighborhood_init) and mysofa_c2s/s2c,
// the oracle for crates/sofa.
//
// Cases:
// - c2s, s2c: Lcg points of varied magnitude.
// - resample_<file>_<rate>: status, N and element counts ("_info"), selected resampled IR rows ("_rows") and the
//   scaled delays ("_delay", when present) for the three files in core/data/hrtf.
// - resample_edges: cipic_124 with a corrupt sampling rate or dimensions: status, N, and checksums.
// - easy_<file>: mysofa_open_data_no_norm at 48 kHz: status and sizes ("_info"), lookup ranges ("_ranges"),
//   Cartesian source positions ("_positions"), neighbourhoods ("_neighbors"), and Lcg queries with the normalised
//   coordinates and indices mysofa_lookup returns ("_query_points", "_query_indices").
// - corpus_<name>: libmysofa's own test files through mysofa_open_data_no_norm: status, sizes and FNV-1a hashes of
//   the resampled IRs, positions and neighbourhoods. Skipped if MYSOFA_TESTS_DIR (default
//   core/deps-build/mysofa/src/mysofa/tests) is missing.
//
// Run from the repository root (paths are relative to it).

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

#include <mysofa.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;

constexpr const char* kFiles[] = {"cipic_124", "sadie_d1", "sadie_h12"};
constexpr float kRates[] = {8000.0f, 22050.0f, 44100.0f, 48000.0f, 88200.0f, 96000.0f};
constexpr float kEasyRate = 48000.0f;
constexpr int kNumCoords = 4001;
constexpr int kNumRows = 6;
constexpr int kNumQueries = 600;

std::vector<char> readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open " + path.string());
    return std::vector<char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

uint64_t fnv1a(const void* data, size_t size)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 0x100000001b3ull;
    }
    return hash;
}

void pushHash(std::vector<int32_t>& row, uint64_t hash)
{
    row.push_back(static_cast<int32_t>(hash & 0xffffffffu));
    row.push_back(static_cast<int32_t>(hash >> 32));
}

MYSOFA_HRTF* load(const std::vector<char>& data)
{
    errno = 0;
    int err = MYSOFA_OK;
    auto* hrtf = mysofa_load_data(data.data(), static_cast<long>(data.size()), &err);
    if (!hrtf)
        throw std::runtime_error("cannot load");
    return hrtf;
}

// A value whose binary exponent is spread over [-20, 20).
float spread(Lcg& lcg)
{
    const float mantissa = lcg.nextSigned();
    const int exponent = static_cast<int>(lcg.nextU32() % 40) - 20;
    return std::ldexp(mantissa, exponent);
}

void generateCoords(const std::filesystem::path& out)
{
    Lcg lcg(61);

    std::vector<float> c2s;
    for (int i = 0; i < kNumCoords; ++i)
    {
        float v[3];
        v[0] = spread(lcg);
        v[1] = spread(lcg);
        v[2] = spread(lcg);
        mysofa_c2s(v);
        c2s.insert(c2s.end(), v, v + 3);
    }
    writeNpy(out / "c2s.npy", {static_cast<size_t>(kNumCoords), 3}, c2s.data());

    std::vector<float> s2c;
    for (int i = 0; i < kNumCoords; ++i)
    {
        float v[3];
        v[0] = lcg.nextSigned() * 540.0f;
        v[1] = lcg.nextSigned() * 120.0f;
        v[2] = spread(lcg);
        mysofa_s2c(v);
        s2c.insert(s2c.end(), v, v + 3);
    }
    writeNpy(out / "s2c.npy", {static_cast<size_t>(kNumCoords), 3}, s2c.data());
}

// Rows of R*N values: the first and last measurement, then Lcg-chosen ones.
std::vector<float> selectRows(const MYSOFA_HRTF* hrtf, Lcg& lcg)
{
    const size_t rowSize = hrtf->R * hrtf->N;
    std::vector<size_t> rows = {0, hrtf->M - 1};
    while (rows.size() < kNumRows)
    {
        const uint32_t row = lcg.nextU32() % hrtf->M;
        rows.push_back(row);
    }
    std::vector<float> values;
    for (size_t row : rows)
        values.insert(values.end(), hrtf->DataIR.values + row * rowSize, hrtf->DataIR.values + (row + 1) * rowSize);
    return values;
}

void generateResample(const std::filesystem::path& out)
{
    Lcg lcg(67);

    for (const char* name : kFiles)
    {
        const auto data = readFile(std::filesystem::path("core/data/hrtf") / (std::string(name) + ".sofa"));
        for (float rate : kRates)
        {
            auto* hrtf = load(data);
            const int status = mysofa_resample(hrtf, rate);
            const std::string stem = "resample_" + std::string(name) + "_" + std::to_string(static_cast<int>(rate));

            std::vector<int32_t> info = {status, static_cast<int32_t>(hrtf->N),
                                         static_cast<int32_t>(hrtf->DataIR.elements),
                                         static_cast<int32_t>(hrtf->DataDelay.elements)};
            writeNpy(out / (stem + "_info.npy"), info);
            const auto rows = selectRows(hrtf, lcg);
            writeNpy(out / (stem + "_rows.npy"), {static_cast<size_t>(kNumRows), hrtf->R * hrtf->N}, rows.data());
            if (hrtf->DataDelay.elements > 0)
                writeNpy(out / (stem + "_delay.npy"), {hrtf->DataDelay.elements}, hrtf->DataDelay.values);
            mysofa_free(hrtf);
        }
    }
}

// cipic_124 (44.1 kHz) with a corrupt sampling rate or dimensions, resampled to 48 kHz unless stated.
void generateResampleEdges(const std::filesystem::path& out)
{
    const auto data = readFile("core/data/hrtf/cipic_124.sofa");
    const float rates[] = {0.5f, -1.5f, std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(), 1.0e6f, 24000.0f, 44100.5f};
    const int numCases = static_cast<int>(std::size(rates)) + 3;

    std::vector<int32_t> rows;
    for (int i = 0; i < numCases; ++i)
    {
        auto* hrtf = load(data);
        float target = 48000.0f;
        if (i < static_cast<int>(std::size(rates)))
            hrtf->DataSamplingRate.values[0] = rates[i];
        else if (i == numCases - 3)
            target = 7999.0f; // Below 8 kHz.
        else if (i == numCases - 2)
            target = 44100.0f; // The file's own rate: nothing to do.
        else
            hrtf->N += 1; // DataIR no longer has M * R * N values.

        const int status = mysofa_resample(hrtf, target);
        rows.insert(rows.end(), {status, static_cast<int32_t>(hrtf->N), static_cast<int32_t>(hrtf->DataIR.elements)});
        pushHash(rows, fnv1a(hrtf->DataIR.values, hrtf->DataIR.elements * sizeof(float)));
        pushHash(rows, fnv1a(hrtf->DataDelay.values, hrtf->DataDelay.elements * sizeof(float)));
        mysofa_free(hrtf);
    }
    writeNpy(out / "resample_edges.npy", {static_cast<size_t>(numCases), 7}, rows.data());
}

void generateEasy(const std::filesystem::path& out)
{
    uint32_t seed = 71;
    for (const char* name : kFiles)
    {
        const auto data = readFile(std::filesystem::path("core/data/hrtf") / (std::string(name) + ".sofa"));
        int err = 0;
        int filterLength = 0;
        errno = 0;
        auto* easy = mysofa_open_data_no_norm(data.data(), static_cast<long>(data.size()), kEasyRate, &filterLength,
                                              &err);
        if (!easy)
            throw std::runtime_error(std::string("cannot open ") + name);

        const std::string stem = "easy_" + std::string(name);
        const auto* hrtf = easy->hrtf;
        const size_t m = hrtf->M;
        writeNpy(out / (stem + "_info.npy"), std::vector<int32_t>{err, filterLength, static_cast<int32_t>(m)});

        auto* lookup = easy->lookup;
        writeNpy(out / (stem + "_ranges.npy"),
                 std::vector<float>{lookup->radius_min, lookup->radius_max, lookup->theta_min, lookup->theta_max,
                                    lookup->phi_min, lookup->phi_max});
        writeNpy(out / (stem + "_positions.npy"), {m, 3}, hrtf->SourcePosition.values);
        writeNpy(out / (stem + "_neighbors.npy"), {m, 6}, easy->neighborhood->index);

        // Queries: random points, measured positions, midpoints of two measurements, scaled measurements, and the
        // zero vector last.
        Lcg lcg(seed++);
        const float* positions = hrtf->SourcePosition.values;
        std::vector<float> points;
        std::vector<int32_t> indices;
        for (int q = 0; q < kNumQueries; ++q)
        {
            float c[3] = {0.0f, 0.0f, 0.0f};
            if (q < kNumQueries - 1)
            {
                const uint32_t kind = lcg.nextU32() % 4;
                if (kind == 0)
                {
                    c[0] = lcg.nextSigned() * 2.0f;
                    c[1] = lcg.nextSigned() * 2.0f;
                    c[2] = lcg.nextSigned() * 2.0f;
                }
                else if (kind == 1)
                {
                    const uint32_t a = lcg.nextU32() % m;
                    std::copy(positions + a * 3, positions + a * 3 + 3, c);
                }
                else if (kind == 2)
                {
                    const uint32_t a = lcg.nextU32() % m;
                    const uint32_t b = lcg.nextU32() % m;
                    for (int k = 0; k < 3; ++k)
                        c[k] = (positions[a * 3 + k] + positions[b * 3 + k]) * 0.5f;
                }
                else
                {
                    const uint32_t a = lcg.nextU32() % m;
                    const float scale = lcg.nextSigned() * 4.0f;
                    for (int k = 0; k < 3; ++k)
                        c[k] = positions[a * 3 + k] * scale;
                }
            }
            indices.push_back(mysofa_lookup(lookup, c));
            points.insert(points.end(), c, c + 3);
        }
        writeNpy(out / (stem + "_query_points.npy"), {static_cast<size_t>(kNumQueries), 3}, points.data());
        writeNpy(out / (stem + "_query_indices.npy"), indices);

        mysofa_close(easy);
    }
}

void generateCorpus(const std::filesystem::path& out)
{
    const char* env = std::getenv("MYSOFA_TESTS_DIR");
    const std::filesystem::path dir = env ? env : "core/deps-build/mysofa/src/mysofa/tests";
    if (!std::filesystem::is_directory(dir))
    {
        std::fprintf(stderr, "  %s not found, skipping the libmysofa corpus\n", dir.string().c_str());
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.path().extension() != ".sofa")
            continue;
        const auto data = readFile(entry.path());
        int err = 0;
        int filterLength = 0;
        errno = 0;
        auto* easy = mysofa_open_data_no_norm(data.data(), static_cast<long>(data.size()), kEasyRate, &filterLength,
                                              &err);
        std::vector<int32_t> row = {err};
        if (easy)
        {
            const auto* hrtf = easy->hrtf;
            row.insert(row.end(), {filterLength, static_cast<int32_t>(hrtf->M)});
            pushHash(row, fnv1a(hrtf->DataIR.values, hrtf->DataIR.elements * sizeof(float)));
            pushHash(row, fnv1a(hrtf->SourcePosition.values, hrtf->SourcePosition.elements * sizeof(float)));
            pushHash(row, fnv1a(easy->neighborhood->index, easy->neighborhood->elements * 6 * sizeof(int)));
            mysofa_close(easy);
        }
        writeNpy(out / ("corpus_" + entry.path().stem().string() + ".npy"), row);
    }
}

void generate(const std::filesystem::path& out)
{
    std::filesystem::create_directories(out);
    generateCoords(out);
    generateResample(out);
    generateResampleEdges(out);
    generateEasy(out);
    generateCorpus(out);
}

Register reg("sofa_easy", generate);

}
