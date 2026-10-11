// Golden data for stages 2.2-2.3: sampling.{h,cpp}, array_math.{h,cpp} (generic, non-IPP path) and DynamicMatrix
// from matrix.h.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/arrays.rs replays
// the same order.

#include <array_math.h>
#include <matrix.h>
#include <sampling.h>
#include <sphere.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;
using complex_t = std::complex<float>;

// Not a multiple of 4, so both the SIMD body and the scalar tail of ArrayMath are exercised.
constexpr int kArraySize = 37;

std::vector<float> randomFloats(Lcg& lcg, int n, float scale)
{
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i)
        out[i] = lcg.nextSigned() * scale;
    return out;
}

std::vector<complex_t> randomComplex(Lcg& lcg, int n, float scale)
{
    std::vector<complex_t> out(n);
    for (int i = 0; i < n; ++i)
    {
        const float re = lcg.nextSigned() * scale;
        out[i] = complex_t(re, lcg.nextSigned() * scale);
    }
    return out;
}

void writeVectors(const std::filesystem::path& path, const std::vector<Vector3f>& v)
{
    writeNpy(path, {v.size(), 3}, reinterpret_cast<const float*>(v.data()));
}

void generateSampling(const std::filesystem::path& out)
{
    constexpr int kNumSamples = 97;

    std::vector<Vector3f> sphere(kNumSamples), hemisphere(kNumSamples), volume(kNumSamples);
    Sampling::generateSphereSamples(kNumSamples, sphere.data());
    Sampling::generateHemisphereSamples(kNumSamples, hemisphere.data());
    Sampling::generateSphereVolumeSamples(kNumSamples, volume.data());
    writeVectors(out / "sphere.npy", sphere);
    writeVectors(out / "hemisphere.npy", hemisphere);
    writeVectors(out / "sphere_volume.npy", volume);

    Lcg lcg(7);
    std::vector<Vector3f> transformedHemisphere(kNumSamples), transformedVolume(kNumSamples);
    for (int i = 0; i < kNumSamples; ++i)
    {
        Vector3f normal;
        normal.x() = lcg.nextSigned();
        normal.y() = lcg.nextSigned();
        normal.z() = lcg.nextSigned();
        normal = Vector3f::unitVector(normal);
        transformedHemisphere[i] = Sampling::transformHemisphereSample(hemisphere[i], normal);

        Sphere sphereVolume;
        sphereVolume.center.x() = lcg.nextSigned() * 5.0f;
        sphereVolume.center.y() = lcg.nextSigned() * 5.0f;
        sphereVolume.center.z() = lcg.nextSigned() * 5.0f;
        sphereVolume.radius = std::abs(lcg.nextSigned()) * 3.0f;
        transformedVolume[i] = Sampling::transformSphereVolumeSample(volume[i], sphereVolume);
    }
    writeVectors(out / "transformed_hemisphere.npy", transformedHemisphere);
    writeVectors(out / "transformed_sphere_volume.npy", transformedVolume);
}

void generateArrayMath(const std::filesystem::path& out)
{
    const int n = kArraySize;
    Lcg lcg(8);
    auto a = randomFloats(lcg, n, 4.0f);
    auto b = randomFloats(lcg, n, 4.0f);
    auto ca = randomComplex(lcg, n, 2.0f);
    auto cb = randomComplex(lcg, n, 2.0f);
    const float s = lcg.nextSigned() * 3.0f;

    // Special values for threshold: the SIMD body and the scalar tail treat NaN and -0.0 differently.
    auto t = a;
    t[1] = std::numeric_limits<float>::quiet_NaN();
    t[2] = -0.0f;
    t[n - 1] = std::numeric_limits<float>::quiet_NaN();
    t[n - 2] = -0.0f;

    std::vector<float> realOut(n);
    std::vector<complex_t> complexOut(n);

    ArrayMath::add(n, a.data(), b.data(), realOut.data());
    writeNpy(out / "add.npy", realOut);
    ArrayMath::add(n, ca.data(), cb.data(), complexOut.data());
    writeNpy(out / "add_complex.npy", complexOut);

    ArrayMath::multiply(n, a.data(), b.data(), realOut.data());
    writeNpy(out / "multiply.npy", realOut);
    ArrayMath::multiply(n, ca.data(), cb.data(), complexOut.data());
    writeNpy(out / "multiply_complex.npy", complexOut);

    realOut = b;
    ArrayMath::multiplyAccumulate(n, a.data(), b.data(), realOut.data());
    writeNpy(out / "multiply_accumulate.npy", realOut);
    complexOut = cb;
    ArrayMath::multiplyAccumulate(n, ca.data(), cb.data(), complexOut.data());
    writeNpy(out / "multiply_accumulate_complex.npy", complexOut);

    ArrayMath::scale(n, a.data(), s, realOut.data());
    writeNpy(out / "scale.npy", realOut);
    ArrayMath::scale(n, ca.data(), s, complexOut.data());
    writeNpy(out / "scale_complex.npy", complexOut);

    realOut = b;
    ArrayMath::scaleAccumulate(n, a.data(), s, realOut.data());
    writeNpy(out / "scale_accumulate.npy", realOut);

    ArrayMath::addConstant(n, a.data(), s, realOut.data());
    writeNpy(out / "add_constant.npy", realOut);

    float maxValue = 0.0f;
    int maxIndex = 0;
    ArrayMath::max(n, a.data(), maxValue);
    std::vector<float> maxOut{maxValue};
    ArrayMath::maxIndex(n, a.data(), maxValue, maxIndex);
    maxOut.push_back(maxValue);
    writeNpy(out / "max.npy", maxOut);
    std::vector<int32_t> maxIndexOut{maxIndex};
    writeNpy(out / "max_index.npy", maxIndexOut);

    ArrayMath::threshold(n, t.data(), 0.0f, realOut.data());
    writeNpy(out / "threshold.npy", realOut);

    std::vector<float> positive(n);
    for (int i = 0; i < n; ++i)
        positive[i] = std::abs(a[i]) + 0.01f;
    ArrayMath::log(n, positive.data(), realOut.data());
    writeNpy(out / "log.npy", realOut);

    ArrayMath::exp(n, a.data(), realOut.data());
    writeNpy(out / "exp.npy", realOut);
    ArrayMath::exp(n, ca.data(), complexOut.data());
    writeNpy(out / "exp_complex.npy", complexOut);

    ArrayMath::magnitude(n, ca.data(), realOut.data());
    writeNpy(out / "magnitude.npy", realOut);
    ArrayMath::phase(n, ca.data(), realOut.data());
    writeNpy(out / "phase.npy", realOut);

    ArrayMath::polarToCartesian(n, positive.data(), a.data(), complexOut.data());
    writeNpy(out / "polar_to_cartesian.npy", complexOut);
}

void writeMatrix(const std::filesystem::path& path, const DynamicMatrixf& m)
{
    // Stored as [cols, rows]: the column-major element buffer as-is.
    writeNpy(path, {static_cast<size_t>(m.numCols), static_cast<size_t>(m.numRows)}, m.elements.data());
}

void generateDynamicMatrix(const std::filesystem::path& out)
{
    Lcg lcg(9);
    auto fill = [&](DynamicMatrixf& m)
    {
        for (int i = 0; i < m.numRows; ++i)
            for (int j = 0; j < m.numCols; ++j)
                m(i, j) = lcg.nextSigned() * 2.0f;
    };

    DynamicMatrixf a(5, 7), b(7, 3), c(5, 7);
    fill(a);
    fill(b);
    fill(c);
    std::vector<float> v(7);
    for (auto& x : v)
        x = lcg.nextSigned();

    DynamicMatrixf product(5, 3);
    multiplyMatrices(a, b, product);
    writeMatrix(out / "product.npy", product);

    std::vector<float> mv(5);
    multiplyMatrixVector(a, v.data(), mv.data());
    writeNpy(out / "mul_vector.npy", mv);

    DynamicMatrixf sum(5, 7), difference(5, 7), scaled(5, 7);
    addMatrices(a, c, sum);
    subtractMatrices(a, c, difference);
    scaleMatrix(a, 0.3f, scaled);
    writeMatrix(out / "sum.npy", sum);
    writeMatrix(out / "difference.npy", difference);
    writeMatrix(out / "scaled.npy", scaled);
}

Register sampling("sampling", generateSampling);
Register arrayMath("array_math", generateArrayMath);
Register dynamicMatrix("dynamic_matrix", generateDynamicMatrix);

}
