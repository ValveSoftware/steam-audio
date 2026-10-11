// Minimal NumPy .npy writer (little-endian, C order). Read on the Rust side by crates/parity.
#pragma once

#include <complex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace golden {

template <typename T> struct NpyDescr;
template <> struct NpyDescr<float> { static constexpr const char* value = "<f4"; };
template <> struct NpyDescr<double> { static constexpr const char* value = "<f8"; };
template <> struct NpyDescr<int32_t> { static constexpr const char* value = "<i4"; };
template <> struct NpyDescr<uint8_t> { static constexpr const char* value = "|u1"; };

template <typename T>
void writeNpy(const std::filesystem::path& path, const std::vector<size_t>& shape, const T* data)
{
    std::string shapeText = "(";
    size_t count = 1;
    for (size_t i = 0; i < shape.size(); ++i)
    {
        shapeText += std::to_string(shape[i]) + (shape.size() == 1 ? "," : (i + 1 < shape.size() ? ", " : ""));
        count *= shape[i];
    }
    shapeText += ")";

    std::string header = std::string("{'descr': '") + NpyDescr<T>::value +
                         "', 'fortran_order': False, 'shape': " + shapeText + ", }";
    const size_t preamble = 6 + 2 + 2;
    const size_t unpadded = preamble + header.size() + 1;
    header.append((64 - unpadded % 64) % 64, ' ');
    header += '\n';

    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw std::runtime_error("cannot open " + path.string());

    const uint16_t headerLen = static_cast<uint16_t>(header.size());
    out.write("\x93NUMPY\x01\x00", 8);
    out.put(static_cast<char>(headerLen & 0xff));
    out.put(static_cast<char>(headerLen >> 8));
    out.write(header.data(), header.size());
    out.write(reinterpret_cast<const char*>(data), count * sizeof(T));
}

template <typename T>
void writeNpy(const std::filesystem::path& path, const std::vector<T>& data)
{
    writeNpy(path, {data.size()}, data.data());
}

// Complex arrays are stored as [..., 2] float arrays (real, imaginary).
inline void writeNpy(const std::filesystem::path& path, const std::vector<std::complex<float>>& data)
{
    writeNpy(path, {data.size(), 2}, reinterpret_cast<const float*>(data.data()));
}

// Deterministic input generator, reproduced bit-exactly on the Rust side (parity::Lcg) so that inputs need not be
// stored. 32-bit LCG (Numerical Recipes constants); returns floats uniformly in [-1, 1).
class Lcg
{
public:
    explicit Lcg(uint32_t seed) : mState(seed) {}

    uint32_t nextU32()
    {
        mState = mState * 1664525u + 1013904223u;
        return mState;
    }

    float nextSigned()
    {
        return static_cast<float>(nextU32() >> 8) * (1.0f / 8388608.0f) - 1.0f;
    }

private:
    uint32_t mState;
};

}
