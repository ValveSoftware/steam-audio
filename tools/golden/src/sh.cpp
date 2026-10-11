// Golden data for stage 2.6: sh.{h,cpp} and sh/spherical_harmonics.{h,cc}.
//
// Inputs come from Lcg draws in a fixed order, one value per statement; crates/steam-audio/tests/sh.rs replays the
// same order.

#include <coordinate_space.h>
#include <math_functions.h>
#include <quaternion.h>
#include <sh.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using namespace ipl;

// Above the Google library's hard-coded limit (order 4), so the recurrence path is exercised too.
constexpr int kMaxOrder = 6;
constexpr int kMaxCoeffs = (kMaxOrder + 1) * (kMaxOrder + 1);
constexpr int kNumRandomDirections = 30;

// Order 5 covers four levels of the band rotation recurrence.
constexpr int kRotationOrder = 5;
constexpr int kRotationCoeffs = (kRotationOrder + 1) * (kRotationOrder + 1);
constexpr int kNumRandomRotations = 15;

// The zero vector (hybrid reverb evaluates SH at Vector3f{}), the six axes, then random unit vectors.
std::vector<Vector3f> directions(Lcg& lcg)
{
    std::vector<Vector3f> dirs = {
        Vector3f(0.0f, 0.0f, 0.0f),
        Vector3f(1.0f, 0.0f, 0.0f), Vector3f(-1.0f, 0.0f, 0.0f),
        Vector3f(0.0f, 1.0f, 0.0f), Vector3f(0.0f, -1.0f, 0.0f),
        Vector3f(0.0f, 0.0f, 1.0f), Vector3f(0.0f, 0.0f, -1.0f),
    };

    for (int i = 0; i < kNumRandomDirections; ++i)
    {
        Vector3f v;
        v.x() = lcg.nextSigned();
        v.y() = lcg.nextSigned();
        v.z() = lcg.nextSigned();
        dirs.push_back(Vector3f::unitVector(v));
    }

    return dirs;
}

void generateSh(const std::filesystem::path& out)
{
    Lcg lcg(17);
    const auto dirs = directions(lcg);
    const auto numDirs = dirs.size();

    std::vector<float> evaluate;
    for (const auto& dir : dirs)
        for (int l = 0; l <= kMaxOrder; ++l)
            for (int m = -l; m <= l; ++m)
                evaluate.push_back(SphericalHarmonics::evaluate(l, m, dir));
    writeNpy(out / "evaluate.npy", {numDirs, static_cast<size_t>(kMaxCoeffs)}, evaluate.data());

    std::vector<float> projected;
    for (const auto& dir : dirs)
    {
        std::vector<float> coeffs(kMaxCoeffs);
        for (auto& c : coeffs)
            c = lcg.nextSigned();
        const float gain = lcg.nextSigned() * 2.0f;
        SphericalHarmonics::projectSinglePointAndUpdate(dir, kMaxOrder, gain, coeffs.data());
        projected.insert(projected.end(), coeffs.begin(), coeffs.end());
    }
    writeNpy(out / "project_single_point_and_update.npy", {numDirs, static_cast<size_t>(kMaxCoeffs)},
             projected.data());

    std::vector<float> sumCoeffs(kMaxCoeffs);
    for (auto& c : sumCoeffs)
        c = lcg.nextSigned();
    std::vector<float> sums;
    for (const auto& dir : dirs)
        for (int order = 0; order <= kMaxOrder; ++order)
            sums.push_back(SphericalHarmonics::evaluateSum(order, sumCoeffs.data(), dir));
    writeNpy(out / "evaluate_sum.npy", {numDirs, static_cast<size_t>(kMaxOrder + 1)}, sums.data());

    // legendre() recurses forever for n >= 4, so only the closed forms are reachable.
    std::vector<float> xs(37);
    for (auto& x : xs)
        x = lcg.nextSigned();
    std::vector<float> legendre;
    for (int n = 0; n <= 3; ++n)
        for (auto x : xs)
            legendre.push_back(SphericalHarmonics::legendre(n, x));
    writeNpy(out / "legendre.npy", {4, xs.size()}, legendre.data());
}

// The Google library's own functions, in its frame and in double precision.
void generateShBasis(const std::filesystem::path& out)
{
    Lcg lcg(19);

    // Poles first: theta = 0 and pi.
    std::vector<double> phis = {0.0, 1.0}, thetas = {0.0, Math::kPiD};
    for (int i = 0; i < 35; ++i)
    {
        const double phi = lcg.nextSigned() * Math::kPiD;
        const double theta = (lcg.nextSigned() + 1.0) * 0.5 * Math::kPiD;
        phis.push_back(phi);
        thetas.push_back(theta);
    }
    const auto n = phis.size();

    std::vector<double> vectors, coords, slow, fast;
    for (size_t i = 0; i < n; ++i)
    {
        const auto v = sh::ToVector(phis[i], thetas[i]);
        vectors.insert(vectors.end(), {v.x(), v.y(), v.z()});

        double phi, theta;
        sh::ToSphericalCoords(v, &phi, &theta);
        coords.insert(coords.end(), {phi, theta});

        for (int l = 0; l <= kMaxOrder; ++l)
        {
            for (int m = -l; m <= l; ++m)
            {
                slow.push_back(sh::EvalSHSlow(l, m, phis[i], thetas[i]));
                fast.push_back(sh::EvalSH(l, m, phis[i], thetas[i]));
            }
        }
    }
    writeNpy(out / "to_vector.npy", {n, 3}, vectors.data());
    writeNpy(out / "to_spherical_coords.npy", {n, 2}, coords.data());
    writeNpy(out / "eval_sh_slow.npy", {n, static_cast<size_t>(kMaxCoeffs)}, slow.data());
    writeNpy(out / "eval_sh.npy", {n, static_cast<size_t>(kMaxCoeffs)}, fast.data());
}

// Per rotation: every band matrix (row by row), then apply() at the full order and at order 3 into an output
// pre-filled with 7, so the untouched tail is visible.
struct RotationTables
{
    std::vector<float> bands, apply, applyOrder3;

    void push(Lcg& lcg, const sh::Rotation& rotation, const SHRotation& shRotation)
    {
        for (int l = 0; l <= kRotationOrder; ++l)
        {
            const auto& band = rotation.band_rotation(l);
            for (int i = 0; i < band.numRows; ++i)
                for (int j = 0; j < band.numCols; ++j)
                    bands.push_back(band(i, j));
        }

        std::vector<float> coeffs(kRotationCoeffs);
        for (auto& c : coeffs)
            c = lcg.nextSigned();

        std::vector<float> rotated(kRotationCoeffs, 7.0f);
        shRotation.apply(kRotationOrder, coeffs.data(), rotated.data());
        apply.insert(apply.end(), rotated.begin(), rotated.end());

        std::fill(rotated.begin(), rotated.end(), 7.0f);
        shRotation.apply(3, coeffs.data(), rotated.data());
        applyOrder3.insert(applyOrder3.end(), rotated.begin(), rotated.end());
    }

    void write(const std::filesystem::path& out, const std::string& prefix) const
    {
        size_t bandWidth = 0;
        for (int l = 0; l <= kRotationOrder; ++l)
            bandWidth += (2 * l + 1) * (2 * l + 1);
        const auto count = apply.size() / kRotationCoeffs;

        writeNpy(out / (prefix + "_bands.npy"), {count, bandWidth}, bands.data());
        writeNpy(out / (prefix + "_apply.npy"), {count, static_cast<size_t>(kRotationCoeffs)}, apply.data());
        writeNpy(out / (prefix + "_apply_order3.npy"), {count, static_cast<size_t>(kRotationCoeffs)},
                 applyOrder3.data());
    }
};

void generateShRotation(const std::filesystem::path& out)
{
    Lcg lcg(23);

    // Coordinate spaces: canonical (what AmbisonicsRotateEffect starts from), then random `ahead` vectors.
    RotationTables spaces;
    for (int i = 0; i <= kNumRandomRotations; ++i)
    {
        CoordinateSpace3f space{};
        if (i > 0)
        {
            Vector3f ahead;
            ahead.x() = lcg.nextSigned();
            ahead.y() = lcg.nextSigned();
            ahead.z() = lcg.nextSigned();
            space = CoordinateSpace3f(Vector3f::unitVector(ahead), Vector3f::kZero);
        }

        sh::Rotation rotation(kRotationOrder);
        rotation.SetRotation(SphericalHarmonics::convertedCoordinateSpace(space));
        SHRotation shRotation(kRotationOrder);
        shRotation.setRotation(space);
        spaces.push(lcg, rotation, shRotation);
    }
    spaces.write(out, "space");

    // Quaternions: identity, then random normalized ones.
    RotationTables quaternions;
    for (int i = 0; i <= kNumRandomRotations; ++i)
    {
        Quaternionf q(0.0f, 0.0f, 0.0f, 1.0f);
        if (i > 0)
        {
            q.x = lcg.nextSigned();
            q.y = lcg.nextSigned();
            q.z = lcg.nextSigned();
            q.w = lcg.nextSigned();
            q.normalize();
        }

        sh::Rotation rotation(kRotationOrder);
        rotation.SetRotation(SphericalHarmonics::convertedQuaternion(q));
        SHRotation shRotation(kRotationOrder);
        shRotation.setRotation(q);
        quaternions.push(lcg, rotation, shRotation);
    }
    quaternions.write(out, "quaternion");
}

Register sh("sh", generateSh);
Register shBasis("sh_basis", generateShBasis);
Register shRotation("sh_rotation", generateShRotation);

}
