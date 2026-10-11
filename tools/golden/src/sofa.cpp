// Golden data for stage 3.5a: libmysofa's loader (mysofa_load_data) and mysofa_check, the oracle for crates/sofa.
//
// Each loaded file is described by a "blob": NUL-terminated text tokens listing the load status, dimensions, check
// status, attributes (in file order) and every array (element count and FNV-1a hash of its float values).
// crates/sofa/tests/sofa.rs builds the same blob from the Rust result and compares it byte for byte.
//
// Cases:
// - <file>_blob, <file>_source_position, <file>_data_ir_rows: the three SOFA files in core/data/hrtf.
// - corpus_<name>: libmysofa's own test files (MYSOFA_TESTS_DIR, default
//   core/deps-build/mysofa/src/mysofa/tests), including its fuzzing regressions. Skipped if the directory is
//   missing; the Rust test skips them too when the files are not available.
// - mutations: cipic_124.sofa with Lcg-chosen byte changes and truncations; one row per case.
//
// Run from the repository root (paths are relative to it).

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include <mysofa.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;

constexpr const char* kFiles[] = {"cipic_124", "sadie_d1", "sadie_h12"};
constexpr int kNumRandomRows = 4;
constexpr int kNumMutations = 256;
constexpr uint32_t kMetadataRegion = 0x10000;

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

std::string hex64(uint64_t value)
{
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

class Blob
{
public:
    void token(const std::string& text)
    {
        bytes.insert(bytes.end(), text.begin(), text.end());
        bytes.push_back(0);
    }

    void number(long long value) { token(std::to_string(value)); }

    std::vector<uint8_t> bytes;
};

// libmysofa prepends to its lists; the blob lists attributes in file order.
void describeAttributes(Blob& blob, const MYSOFA_ATTRIBUTE* list)
{
    std::vector<const MYSOFA_ATTRIBUTE*> attributes;
    for (auto* attribute = list; attribute; attribute = attribute->next)
        attributes.push_back(attribute);

    for (auto it = attributes.rbegin(); it != attributes.rend(); ++it)
    {
        if ((*it)->value)
        {
            blob.token("attr");
            blob.token((*it)->name);
            blob.token((*it)->value);
        }
        else
        {
            blob.token("attr-null");
            blob.token((*it)->name);
        }
    }
}

void describeArray(Blob& blob, const char* kind, const char* name, const MYSOFA_ARRAY& array)
{
    blob.token(kind);
    blob.token(name);
    if (array.values)
    {
        blob.token("values");
        blob.number(array.elements);
        blob.token(hex64(fnv1a(array.values, array.elements * sizeof(float))));
    }
    else
    {
        blob.token("values-null");
    }
    describeAttributes(blob, array.attributes);
}

Blob describe(MYSOFA_HRTF* hrtf, int err)
{
    Blob blob;
    blob.token("load");
    blob.number(err);
    if (!hrtf)
    {
        blob.token("null");
        return blob;
    }

    blob.token("dims");
    for (unsigned dim : {hrtf->I, hrtf->C, hrtf->R, hrtf->E, hrtf->N, hrtf->M})
        blob.number(dim);
    blob.token("check");
    blob.number(mysofa_check(hrtf));

    describeAttributes(blob, hrtf->attributes);
    describeArray(blob, "array", "ListenerPosition", hrtf->ListenerPosition);
    describeArray(blob, "array", "ReceiverPosition", hrtf->ReceiverPosition);
    describeArray(blob, "array", "SourcePosition", hrtf->SourcePosition);
    describeArray(blob, "array", "EmitterPosition", hrtf->EmitterPosition);
    describeArray(blob, "array", "ListenerUp", hrtf->ListenerUp);
    describeArray(blob, "array", "ListenerView", hrtf->ListenerView);
    describeArray(blob, "array", "Data.IR", hrtf->DataIR);
    describeArray(blob, "array", "Data.SamplingRate", hrtf->DataSamplingRate);
    describeArray(blob, "array", "Data.Delay", hrtf->DataDelay);
    for (auto* variable = hrtf->variables; variable; variable = variable->next)
        describeArray(blob, "var", variable->name, *variable->value);
    return blob;
}

// errno is part of libmysofa's results (it returns errno after a failed seek), so start every load from 0.
MYSOFA_HRTF* load(const std::vector<char>& data, int* err)
{
    errno = 0;
    *err = MYSOFA_OK;
    return mysofa_load_data(data.data(), data.size(), err);
}

void writeBlob(const std::filesystem::path& path, const Blob& blob)
{
    writeNpy(path, blob.bytes);
}

void generateFiles(const std::filesystem::path& out)
{
    Lcg lcg(41);

    for (const char* name : kFiles)
    {
        const auto data = readFile(std::filesystem::path("core/data/hrtf") / (std::string(name) + ".sofa"));
        int err = 0;
        auto* hrtf = load(data, &err);
        if (!hrtf || err != MYSOFA_OK)
            throw std::runtime_error(std::string("cannot load ") + name);
        writeBlob(out / (std::string(name) + "_blob.npy"), describe(hrtf, err));

        const size_t m = hrtf->M;
        const size_t rowSize = hrtf->R * hrtf->N;
        writeNpy(out / (std::string(name) + "_source_position.npy"), {m, 3}, hrtf->SourcePosition.values);

        // First and last measurement, then random ones.
        std::vector<size_t> rows = {0, m - 1};
        for (int i = 0; i < kNumRandomRows; ++i)
        {
            const uint32_t row = lcg.nextU32() % static_cast<uint32_t>(m);
            rows.push_back(row);
        }
        std::vector<float> ir;
        for (size_t row : rows)
            ir.insert(ir.end(), hrtf->DataIR.values + row * rowSize, hrtf->DataIR.values + (row + 1) * rowSize);
        writeNpy(out / (std::string(name) + "_data_ir_rows.npy"), {rows.size(), rowSize}, ir.data());

        mysofa_free(hrtf);
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

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.path().extension() == ".sofa")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& path : files)
    {
        const auto data = readFile(path);
        int err = 0;
        auto* hrtf = load(data, &err);
        writeBlob(out / ("corpus_" + path.stem().string() + ".npy"), describe(hrtf, err));
        mysofa_free(hrtf);
    }
}

void generateMutations(const std::filesystem::path& out)
{
    const auto original = readFile("core/data/hrtf/cipic_124.sofa");
    const auto size = static_cast<uint32_t>(original.size());
    const uint32_t metadata = std::min(size, kMetadataRegion);

    Lcg lcg(53);
    std::vector<int32_t> rows;
    for (int i = 0; i < kNumMutations; ++i)
    {
        auto data = original;
        const uint32_t kind = lcg.nextU32() % 4;
        uint32_t position = 0;
        uint32_t value = 0;
        switch (kind)
        {
        case 0: // Flip bits of a metadata byte.
            position = lcg.nextU32() % metadata;
            value = lcg.nextU32() % 255 + 1;
            data[position] = static_cast<char>(data[position] ^ value);
            break;
        case 1: // Flip bits of any byte.
            position = lcg.nextU32() % size;
            value = lcg.nextU32() % 255 + 1;
            data[position] = static_cast<char>(data[position] ^ value);
            break;
        case 2: // Overwrite a metadata byte.
            position = lcg.nextU32() % metadata;
            value = lcg.nextU32() % 256;
            data[position] = static_cast<char>(value);
            break;
        default: // Truncate.
            position = lcg.nextU32() % size;
            data.resize(position);
            break;
        }

        int err = 0;
        auto* hrtf = load(data, &err);
        const auto blob = describe(hrtf, err);
        const uint64_t hash = fnv1a(blob.bytes.data(), blob.bytes.size());
        rows.insert(rows.end(), {static_cast<int32_t>(kind), static_cast<int32_t>(position), static_cast<int32_t>(value),
                                 err, hrtf ? 1 : 0, static_cast<int32_t>(hash & 0xffffffffu),
                                 static_cast<int32_t>(hash >> 32)});
        mysofa_free(hrtf);
    }
    writeNpy(out / "mutations.npy", {static_cast<size_t>(kNumMutations), 7}, rows.data());
}

void generate(const std::filesystem::path& out)
{
    std::filesystem::create_directories(out);
    generateFiles(out);
    generateCorpus(out);
    generateMutations(out);
}

Register reg("sofa", generate);

}
