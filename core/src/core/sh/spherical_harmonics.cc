//
// Based on the Google spherical harmonics library: 
// https://github.com/google/spherical-harmonics
//
// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "spherical_harmonics.h"

#include <random>

#include "../math_functions.h"

using namespace ipl;

namespace sh
{
    namespace
    {
        // Number of precomputed factorials and double-factorials that can be
        // returned in constant time.
        const int kCacheSize = 16;

        const int kHardCodedOrderLimit = 4;

        // For efficiency, the cosine lobe for normal = (0, 0, 1) as the first 9
        // spherical harmonic coefficients are hardcoded below. This was computed by
        // evaluating:
        //   ProjectFunction(kIrradianceOrder, [] (double phi, double theta) {
        //     return Clamp(Eigen::Vector3d::UnitZ().dot(ToVector(phi, theta)), 
        //                  0.0, 1.0);
        //   }, 10000000);
        const std::vector<double> cosine_lobe = { 0.886227, 0.0, 1.02333, 0.0, 0.0, 0.0,
            0.495416, 0.0, 0.0 };

        // A zero template is required for EvalSHSum to handle its template
        // instantiations and a type's default constructor does not necessarily
        // initialize to zero.
        template<typename T> T Zero();
        template<> double Zero() { return 0.0; }
        template<> float Zero() { return 0.0; }

        /*
        template <class T>
        using VectorX = Eigen::Matrix<T, Eigen::Dynamic, 1>;
        */

        // Usage: CHECK(bool, string message);
        // Note that it must end a semi-colon, making it look like a
        // valid C++ statement (hence the awkward do() while(false)).
        //#ifndef NDEBUG
        //# define CHECK(condition, message) \
        //  do { \
        //    if (!(condition)) { \
        //      std::cerr << "Check failed (" #condition ") in " << __FILE__ \
        //        << ":" << __LINE__ << ", message: " << message << std::endl; \
        //      std::exit(EXIT_FAILURE); \
        //    } \
        //  } while(false)
        //#else
        # define CHECK(condition, message) do {} while(false)
        //#endif

        // Clamp the first argument to be greater than or equal to the second
        // and less than or equal to the third.
        double Clamp(double val, double min, double max) {
            if (val < min) {
                val = min;
            }
            if (val > max) {
                val = max;
            }
            return val;
        }

        // Return true if the first value is within epsilon of the second value.
        bool NearByMargin(double actual, double expected) {
            double diff = actual - expected;
            if (diff < 0.0) {
                diff = -diff;
            }
            // 5 bits of error in mantissa (source of '32 *')
            return diff < 32 * std::numeric_limits<double>::epsilon();
        }

        // Return floating mod x % m.
#if defined(IPL_OS_MACOSX)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
        double FastFMod(double x, double m) {
            return x - (m * floor(x / m));
        }
#pragma clang diagnostic pop
#endif

        // Hardcoded spherical harmonic functions for low orders (l is first number
        // and m is second number (sign encoded as preceeding 'p' or 'n')).
        //
        // As polynomials they are evaluated more efficiently in cartesian coordinates,
        // assuming that @d is unit. This is not verified for efficiency.
        //
        // NOTE: Condon-Shortley phase has been removed from the below definitions
        //       of SH basis functions.
        double HardcodedSH00(const Vector3d& d) {
            // 0.5 * sqrt(1/pi)
            return 0.282095;
        }

        double HardcodedSH1n1(const Vector3d& d) {
            // sqrt(3/(4pi)) * y
            return 0.488603 * d.y();
        }

        double HardcodedSH10(const Vector3d& d) {
            // sqrt(3/(4pi)) * z
            return 0.488603 * d.z();
        }

        double HardcodedSH1p1(const Vector3d& d) {
            // sqrt(3/(4pi)) * x
            return 0.488603 * d.x();
        }

        double HardcodedSH2n2(const Vector3d& d) {
            // 0.5 * sqrt(15/pi) * x * y
            return 1.092548 * d.x() * d.y();
        }

        double HardcodedSH2n1(const Vector3d& d) {
            // 0.5 * sqrt(15/pi) * y * z
            return 1.092548 * d.y() * d.z();
        }

        double HardcodedSH20(const Vector3d& d) {
            // 0.25 * sqrt(5/pi) * (-x^2-y^2+2z^2)
            return 0.315392 * (-d.x() * d.x() - d.y() * d.y() + 2.0 * d.z() * d.z());
        }

        double HardcodedSH2p1(const Vector3d& d) {
            // 0.5 * sqrt(15/pi) * x * z
            return 1.092548 * d.x() * d.z();
        }

        double HardcodedSH2p2(const Vector3d& d) {
            // 0.25 * sqrt(15/pi) * (x^2 - y^2)
            return 0.546274 * (d.x() * d.x() - d.y() * d.y());
        }

        double HardcodedSH3n3(const Vector3d& d) {
            // 0.25 * sqrt(35/(2pi)) * y * (3x^2 - y^2)
            return 0.590044 * d.y() * (3.0 * d.x() * d.x() - d.y() * d.y());
        }

        double HardcodedSH3n2(const Vector3d& d) {
            // 0.5 * sqrt(105/pi) * x * y * z
            return 2.890611 * d.x() * d.y() * d.z();
        }

        double HardcodedSH3n1(const Vector3d& d) {
            // 0.25 * sqrt(21/(2pi)) * y * (4z^2-x^2-y^2)
            return 0.457046 * d.y() * (4.0 * d.z() * d.z() - d.x() * d.x()
                - d.y() * d.y());
        }

        double HardcodedSH30(const Vector3d& d) {
            // 0.25 * sqrt(7/pi) * z * (2z^2 - 3x^2 - 3y^2)
            return 0.373176 * d.z() * (2.0 * d.z() * d.z() - 3.0 * d.x() * d.x()
                - 3.0 * d.y() * d.y());
        }

        double HardcodedSH3p1(const Vector3d& d) {
            // 0.25 * sqrt(21/(2pi)) * x * (4z^2-x^2-y^2)
            return 0.457046 * d.x() * (4.0 * d.z() * d.z() - d.x() * d.x()
                - d.y() * d.y());
        }

        double HardcodedSH3p2(const Vector3d& d) {
            // 0.25 * sqrt(105/pi) * z * (x^2 - y^2)
            return 1.445306 * d.z() * (d.x() * d.x() - d.y() * d.y());
        }

        double HardcodedSH3p3(const Vector3d& d) {
            // 0.25 * sqrt(35/(2pi)) * x * (x^2-3y^2)
            return 0.590044 * d.x() * (d.x() * d.x() - 3.0 * d.y() * d.y());
        }

        double HardcodedSH4n4(const Vector3d& d) {
            // 0.75 * sqrt(35/pi) * x * y * (x^2-y^2)
            return 2.503343 * d.x() * d.y() * (d.x() * d.x() - d.y() * d.y());
        }

        double HardcodedSH4n3(const Vector3d& d) {
            // 0.75 * sqrt(35/(2pi)) * y * z * (3x^2-y^2)
            return 1.770131 * d.y() * d.z() * (3.0 * d.x() * d.x() - d.y() * d.y());
        }

        double HardcodedSH4n2(const Vector3d& d) {
            // 0.75 * sqrt(5/pi) * x * y * (7z^2-1)
            return 0.946175 * d.x() * d.y() * (7.0 * d.z() * d.z() - 1.0);
        }

        double HardcodedSH4n1(const Vector3d& d) {
            // 0.75 * sqrt(5/(2pi)) * y * z * (7z^2-3)
            return 0.669047 * d.y() * d.z() * (7.0 * d.z() * d.z() - 3.0);
        }

        double HardcodedSH40(const Vector3d& d) {
            // 3/16 * sqrt(1/pi) * (35z^4-30z^2+3)
            double z2 = d.z() * d.z();
            return 0.105786 * (35.0 * z2 * z2 - 30.0 * z2 + 3.0);
        }

        double HardcodedSH4p1(const Vector3d& d) {
            // 0.75 * sqrt(5/(2pi)) * x * z * (7z^2-3)
            return 0.669047 * d.x() * d.z() * (7.0 * d.z() * d.z() - 3.0);
        }

        double HardcodedSH4p2(const Vector3d& d) {
            // 3/8 * sqrt(5/pi) * (x^2 - y^2) * (7z^2 - 1)
            return 0.473087 * (d.x() * d.x() - d.y() * d.y())
                * (7.0 * d.z() * d.z() - 1.0);
        }

        double HardcodedSH4p3(const Vector3d& d) {
            // 0.75 * sqrt(35/(2pi)) * x * z * (x^2 - 3y^2)
            return 1.770131 * d.x() * d.z() * (d.x() * d.x() - 3.0 * d.y() * d.y());
        }

        double HardcodedSH4p4(const Vector3d& d) {
            // 3/16*sqrt(35/pi) * (x^2 * (x^2 - 3y^2) - y^2 * (3x^2 - y^2))
            double x2 = d.x() * d.x();
            double y2 = d.y() * d.y();
            return 0.625836 * (x2 * (x2 - 3.0 * y2) - y2 * (3.0 * x2 - y2));
        }

        // Compute the factorial for an integer @x. It is assumed x is at least 0.
        // This implementation precomputes the results for low values of x, in which
        // case this is a constant time lookup.
        //
        // The vast majority of SH evaluations will hit these precomputed values.
        double Factorial(int x) {
            const double factorial_cache[kCacheSize] = { 1, 1, 2, 6, 24, 120, 720, 5040,
                40320, 362880, 3628800, 39916800,
                479001600, 6227020800,
                87178291200, 1307674368000 };

            if (x < kCacheSize) {
                return factorial_cache[x];
            }
            else {
                double s = 1.0;
                for (int n = 2; n <= x; n++) {
                    s *= n;
                }
                return s;
            }
        }

        // Compute the double factorial for an integer @x. This assumes x is at least
        // 0.  This implementation precomputes the results for low values of x, in
        // which case this is a constant time lookup.
        //
        // The vast majority of SH evaluations will hit these precomputed values.
        // See http://mathworld.wolfram.com/DoubleFactorial.html
        double DoubleFactorial(int x) {
            const double dbl_factorial_cache[kCacheSize] = { 1, 1, 2, 3, 8, 15, 48, 105,
                384, 945, 3840, 10395, 46080,
                135135, 645120, 2027025 };

            if (x < kCacheSize) {
                return dbl_factorial_cache[x];
            }
            else {
                double s = 1.0;
                double n = x;
                while (n > 1.0) {
                    s *= n;
                    n -= 2.0;
                }
                return s;
            }
        }

        // Evaluate the associated Legendre polynomial of degree @l and order @m at
        // coordinate @x. The inputs must satisfy:
        // 1. l >= 0
        // 2. 0 <= m <= l
        // 3. -1 <= x <= 1
        // See http://en.wikipedia.org/wiki/Associated_Legendre_polynomials
        //
        // This implementation is based off the approach described in [1],
        // instead of computing Pml(x) directly, Pmm(x) is computed. Pmm can be
        // lifted to Pmm+1 recursively until Pml is found
        //
        // NOTE: Condon-Shortley phase has been removed from the definition of Pmm.
        double EvalLegendrePolynomial(int l, int m, double x) {
            // Compute Pmm(x) = (2m - 1)!!(1 - x^2)^(m/2), where !! is the double
            // factorial.
            double pmm = 1.0;
            // P00 is defined as 1.0, do don't evaluate Pmm unless we know m > 0
            if (m > 0) {
                pmm = DoubleFactorial(2 * m - 1) * pow(1 - x * x, m / 2.0);
            }

            if (l == m) {
                // Pml is the same as Pmm so there's no lifting to higher bands needed
                return pmm;
            }

            // Compute Pmm+1(x) = x(2m + 1)Pmm(x)
            double pmm1 = x * (2 * m + 1) * pmm;
            if (l == m + 1) {
                // Pml is the same as Pmm+1 so we are done as well
                return pmm1;
            }

            // Use the last two computed bands to lift up to the next band until l is
            // reached, using the recurrence relationship:
            // Pml(x) = (x(2l - 1)Pml-1 - (l + m - 1)Pml-2) / (l - m)
            for (int n = m + 2; n <= l; n++) {
                double pmn = (x * (2 * n - 1) * pmm1 - (n + m - 1) * pmm) / (n - m);
                pmm = pmm1;
                pmm1 = pmn;
            }
            // Pmm1 at the end of the above loop is equal to Pml
            return pmm1;
        }

        // ---- The following functions are used to implement SH rotation computations
        //      based on the recursive approach described in [1, 4]. The names of the
        //      functions correspond with the notation used in [1, 4].

        // See http://en.wikipedia.org/wiki/Kronecker_delta
        double KroneckerDelta(int i, int j) {
            if (i == j) {
                return 1.0;
            }
            else {
                return 0.0;
            }
        }

        // [4] uses an odd convention of referring to the rows and columns using
        // centered indices, so the middle row and column are (0, 0) and the upper
        // left would have negative coordinates.
        //
        // This is a convenience function to allow us to access an DynamicMatrixd
        // in the same manner, assuming r is a (2l+1)x(2l+1) matrix.
        double GetCenteredElement(const DynamicMatrixf& r, int i, int j) 
        {
            // The shift to go from [-l, l] to [0, 2l] is (rows - 1) / 2 = l,
            // (since the matrix is assumed to be square, rows == cols).
            int offset = (static_cast<int>(r.numRows) - 1) / 2;
            return r(i + offset, j + offset);
        }

        // P is a helper function defined in [4] that is used by the functions U, V, W.
        // This should not be called on its own, as U, V, and W (and their coefficients)
        // select the appropriate matrix elements to access (arguments @a and @b).
        double P(int i, int a, int b, int l, const vector<DynamicMatrixf>& r)
        {
            if (b == l) 
            {
                return GetCenteredElement(r[1], i, 1) * GetCenteredElement(r[l - 1], a, l - 1) - GetCenteredElement(r[1], i, -1) * 
                    GetCenteredElement(r[l - 1], a, -l + 1);
            }
            else if (b == -l) 
            {
                return GetCenteredElement(r[1], i, 1) * GetCenteredElement(r[l - 1], a, -l + 1) + GetCenteredElement(r[1], i, -1) * 
                    GetCenteredElement(r[l - 1], a, l - 1);
            }
            else 
            {
                return GetCenteredElement(r[1], i, 0) * GetCenteredElement(r[l - 1], a, b);
            }
        }

        // The functions U, V, and W should only be called if the correspondingly
        // named coefficient u, v, w from the function ComputeUVWCoeff() is non-zero.
        // When the coefficient is 0, these would attempt to access matrix elements that
        // are out of bounds. The list of rotations, @r, must have the @l - 1
        // previously completed band rotations. These functions are valid for l >= 2.

        double U(int m, int n, int l, const vector<DynamicMatrixf>& r)
        {
            // Although [1, 4] split U into three cases for m == 0, m < 0, m > 0
            // the actual values are the same for all three cases
            return P(0, m, n, l, r);
        }

        double V(int m, int n, int l, const vector<DynamicMatrixf>& r)
        {
            if (m == 0) 
            {
                return P(1, 1, n, l, r) + P(-1, -1, n, l, r);
            }
            else if (m > 0) 
            {
                return P(1, m - 1, n, l, r) * sqrt(1 + KroneckerDelta(m, 1)) - P(-1, -m + 1, n, l, r) * (1 - KroneckerDelta(m, 1));
            }
            else 
            {
                // Note there is apparent errata in [1,4,4b] dealing with this particular
                // case. [4b] writes it should be P*(1-d)+P*(1-d)^0.5
                // [1] writes it as P*(1+d)+P*(1-d)^0.5, but going through the math by hand,
                // you must have it as P*(1-d)+P*(1+d)^0.5 to form a 2^.5 term, which
                // parallels the case where m > 0.
                return P(1, m + 1, n, l, r) * (1 - KroneckerDelta(m, -1)) + P(-1, -m - 1, n, l, r) * sqrt(1 + KroneckerDelta(m, -1));
            }
        }

        double W(int m, int n, int l, const vector<DynamicMatrixf>& r) 
        {
            if (m == 0) 
            {
                // whenever this happens, w is also 0 so W can be anything
                return 0.0;
            }
            else if (m > 0) 
            {
                return P(1, m + 1, n, l, r) + P(-1, -m - 1, n, l, r);
            }
            else 
            {
                return P(1, m - 1, n, l, r) - P(-1, -m + 1, n, l, r);
            }
        }


        // Calculate the coefficients applied to the U, V, and W functions. Because
        // their equations share many common terms they are computed simultaneously.
        void ComputeUVWCoeff(int m, int n, int l, double* u, double* v, double* w) {
            double d = KroneckerDelta(m, 0);
            double denom = (abs(n) == l ? 2.0 * l * (2.0 * l - 1) : (l + n) * (l - n));

            *u = sqrt((l + m) * (l - m) / denom);
            *v = 0.5 * sqrt((1 + d) * (l + abs(m) - 1.0) * (l + abs(m)) / denom)
                * (1 - 2 * d);
            *w = -0.5 * sqrt((l - abs(m) - 1) * (l - abs(m)) / denom) * (1 - d);
        }

        // Calculate the (2l+1)x(2l+1) rotation matrix for the band @l.
        // This uses the matrices computed for band 1 and band l-1 to compute the
        // matrix for band l. @rotations must contain the previously computed l-1
        // rotation matrices, and the new matrix for band l will be appended to it.
        //
        // This implementation comes from p. 5 (6346), Table 1 and 2 in [4] taking
        // into account the corrections from [4b].
        void ComputeBandRotation(int l, vector<DynamicMatrixf>* rotations) 
        {
            // The band's rotation matrix has rows and columns equal to the number of
            // coefficients within that band (-l <= m <= l implies 2l + 1 coefficients).
            for (int m = -l; m <= l; m++) 
            {
                for (int n = -l; n <= l; n++) 
                {
                    double u, v, w;
                    ComputeUVWCoeff(m, n, l, &u, &v, &w);

                    // The functions U, V, W are only safe to call if the coefficients
                    // u, v, w are not zero
                    if (!NearByMargin(u, 0.0))
                        u *= U(m, n, l, *rotations);
                    if (!NearByMargin(v, 0.0))
                        v *= V(m, n, l, *rotations);
                    if (!NearByMargin(w, 0.0))
                        w *= W(m, n, l, *rotations);

                    ((*rotations)[l])(m + l, n + l) = static_cast<float>(u + v + w);
                }
            }
        }

    } // namespace

    Vector3d ToVector(const double phi, const double theta) {
        double r = sin(theta);
        return Vector3d(r * cos(phi), r * sin(phi), cos(theta));
    }

    void ToSphericalCoords(const Vector3d& dir, double* phi, double* theta) {
        CHECK(NearByMargin(dir.length(), 1.0), "dir is not unit");
        //CHECK(NearByMargin(dir.squaredNorm(), 1.0), "dir is not unit");
            
        // Explicitly clamp the z coordinate so that numeric errors don't cause it
        // to fall just outside of acos' domain.
        *theta = acos(Clamp(dir.z(), -1.0, 1.0));
        // We don't need to divide dir.y() or dir.x() by sin(theta) since they are
        // both scaled by it and atan2 will handle it appropriately.
        *phi = atan2(dir.y(), dir.x());
    }

    double EvalSHSlow(const int l, const int m, const double phi, const double theta) {
        CHECK(l >= 0, "l must be at least 0.");
        CHECK(-l <= m && m <= l, "m must be between -l and l.");

        double kml = sqrt((2.0 * l + 1) * Factorial(l - abs(m)) /
            (4.0 * Math::kPiD * Factorial(l + abs(m))));
        if (m > 0) {
            return sqrt(2.0) * kml * cos(m * phi) *
                EvalLegendrePolynomial(l, m, cos(theta));
        }
        else if (m < 0) {
            return sqrt(2.0) * kml * sin(-m * phi) *
                EvalLegendrePolynomial(l, -m, cos(theta));
        }
        else {
            return kml * EvalLegendrePolynomial(l, 0, cos(theta));
        }
    }

    double EvalSHSlow(const int l, const int m, const Vector3d& dir) {
        double phi, theta;
        ToSphericalCoords(dir, &phi, &theta);
        return EvalSH(l, m, phi, theta);
    }

    double EvalSH(const int l, const int m, const double phi, const double theta) {
        // If using the hardcoded functions, switch to cartesian
        if (l <= kHardCodedOrderLimit) {
            return EvalSH(l, m, ToVector(phi, theta));
        }
        else {
            // Stay in spherical coordinates since that's what the recurrence
            // version is implemented in
            return EvalSHSlow(l, m, phi, theta);
        }
    }

    double EvalSH(const int l, const int m, const Vector3d& dir) {
        if (l <= kHardCodedOrderLimit) {
            // Validate l and m here (don't do it generally since EvalSHSlow also
            // checks it if we delegate to that function).
            CHECK(l >= 0, "l must be at least 0.");
            CHECK(-l <= m && m <= l, "m must be between -l and l.");
            CHECK(NearByMargin(dir.length(), 1.0), "dir is not unit.");

            switch (l) {
            case 0:
                return HardcodedSH00(dir);
            case 1:
                switch (m) {
                case -1:
                    return HardcodedSH1n1(dir);
                case 0:
                    return HardcodedSH10(dir);
                case 1:
                    return HardcodedSH1p1(dir);
                }
            case 2:
                switch (m) {
                case -2:
                    return HardcodedSH2n2(dir);
                case -1:
                    return HardcodedSH2n1(dir);
                case 0:
                    return HardcodedSH20(dir);
                case 1:
                    return HardcodedSH2p1(dir);
                case 2:
                    return HardcodedSH2p2(dir);
                }
            case 3:
                switch (m) {
                case -3:
                    return HardcodedSH3n3(dir);
                case -2:
                    return HardcodedSH3n2(dir);
                case -1:
                    return HardcodedSH3n1(dir);
                case 0:
                    return HardcodedSH30(dir);
                case 1:
                    return HardcodedSH3p1(dir);
                case 2:
                    return HardcodedSH3p2(dir);
                case 3:
                    return HardcodedSH3p3(dir);
                }
            case 4:
                switch (m) {
                case -4:
                    return HardcodedSH4n4(dir);
                case -3:
                    return HardcodedSH4n3(dir);
                case -2:
                    return HardcodedSH4n2(dir);
                case -1:
                    return HardcodedSH4n1(dir);
                case 0:
                    return HardcodedSH40(dir);
                case 1:
                    return HardcodedSH4p1(dir);
                case 2:
                    return HardcodedSH4p2(dir);
                case 3:
                    return HardcodedSH4p3(dir);
                case 4:
                    return HardcodedSH4p4(dir);
                }
            }

            // This is unreachable given the CHECK's above but the compiler can't tell.
            return 0.0;
        }
        else {
            // Not hard-coded so use the recurrence relation (which will convert this
            // to spherical coordinates).
            return EvalSHSlow(l, m, dir);
        }
    }

    std::unique_ptr<std::vector<double>> ProjectFunction(
        const int order, const SphericalFunction& func, const int sample_count) {
        CHECK(order >= 0, "Order must be at least zero.");
        CHECK(sample_count > 0, "Sample count must be at least one.");

        // This is the approach demonstrated in [1] and is useful for arbitrary
        // functions on the sphere that are represented analytically.
        const int sample_side = static_cast<int>(floor(sqrt(sample_count)));
        std::unique_ptr<std::vector<double>> coeffs(new std::vector<double>());
        coeffs->assign(GetCoefficientCount(order), 0.0);

        // generate sample_side^2 uniformly and stratified samples over the sphere
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> rng(0.0, 1.0);
        for (int t = 0; t < sample_side; t++) {
            for (int p = 0; p < sample_side; p++) {
                double alpha = (t + rng(gen)) / sample_side;
                double beta = (p + rng(gen)) / sample_side;
                // See http://www.bogotobogo.com/Algorithms/uniform_distribution_sphere.php
                double phi = 2.0 * Math::kPiD * beta;
                double theta = acos(2.0 * alpha - 1.0);

                // evaluate the analytic function for the current spherical coords
                double func_value = func(phi, theta);

                // evaluate the SH basis functions up to band O, scale them by the
                // function's value and accumulate them over all generated samples
                for (int l = 0; l <= order; l++) {
                    for (int m = -l; m <= l; m++) {
                        double sh = EvalSH(l, m, phi, theta);
                        (*coeffs)[GetIndex(l, m)] += func_value * sh;
                    }
                }
            }
        }

        // scale by the probability of a particular sample, which is
        // 4pi/sample_side^2. 4pi for the surface area of a unit sphere, and
        // 1/sample_side^2 for the number of samples drawn uniformly.
        double weight = 4.0 * Math::kPiD / (sample_side * sample_side);
        for (unsigned int i = 0; i < coeffs->size(); i++) {
            (*coeffs)[i] *= weight;
        }

        return coeffs;
    }

    void ProjectFunction(const int order, const SphericalFunction& func,
        const int sample_count, double* coeffs) {
        CHECK(order >= 0, "Order must be at least zero.");
        CHECK(sample_count > 0, "Sample count must be at least one.");

        // This is the approach demonstrated in [1] and is useful for arbitrary
        // functions on the sphere that are represented analytically.
        const int sample_side = static_cast<int>(floor(sqrt(sample_count)));
        auto numCoeffs = GetCoefficientCount(order);
        memset(coeffs, 0, numCoeffs * sizeof(float));

        // generate sample_side^2 uniformly and stratified samples over the sphere
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> rng(0.0, 1.0);
        for (int t = 0; t < sample_side; t++) {
            for (int p = 0; p < sample_side; p++) {
                double alpha = (t + rng(gen)) / sample_side;
                double beta = (p + rng(gen)) / sample_side;
                // See http://www.bogotobogo.com/Algorithms/uniform_distribution_sphere.php
                double phi = 2.0 * Math::kPiD * beta;
                double theta = acos(2.0 * alpha - 1.0);

                // evaluate the analytic function for the current spherical coords
                double func_value = func(phi, theta);

                // evaluate the SH basis functions up to band O, scale them by the
                // function's value and accumulate them over all generated samples
                for (int l = 0; l <= order; l++) {
                    for (int m = -l; m <= l; m++) {
                        double sh = EvalSH(l, m, phi, theta);
                        coeffs[GetIndex(l, m)] += func_value * sh;
                    }
                }
            }
        }

        // scale by the probability of a particular sample, which is
        // 4pi/sample_side^2. 4pi for the surface area of a unit sphere, and
        // 1/sample_side^2 for the number of samples drawn uniformly.
        double weight = 4.0 * Math::kPiD / (sample_side * sample_side);
        for (int i = 0; i < numCoeffs; i++) {
            coeffs[i] *= weight;
        }
    }

    void ProjectFunction(const int order, const int sample_count, 
        const Vector3f* samples, const float* sampleValues, float* coefficients)
    {
        CHECK(order >= 0, "Order must be at least zero.");
        CHECK(samples.size() > 0, "Sample count must be at least one.");

        auto numCoeffs = GetCoefficientCount(order);
        memset(coefficients, 0, numCoeffs * sizeof(float));

        for (auto i = 0; i < sample_count; ++i)
            for (auto l = 0; l <= order; ++l)
                for (auto m = -l; m <= l; ++m)
                    coefficients[GetIndex(l, m)] += sampleValues[i] * 
                        static_cast<float>(EvalSH(l, m, Vector3d(samples[i].x(), samples[i].y(), samples[i].z())));

        auto scalar = 4.0f * Math::kPi / sample_count;
        for (auto i = 0; i < numCoeffs; ++i)
            coefficients[i] *= scalar;
    }

#if (defined(IPL_USE_MKL))
    std::unique_ptr<std::vector<double>> ProjectSparseSamples(
        const int order, const std::vector<Vector3d>& dirs,
        const std::vector<double>& values) {
        CHECK(order >= 0, "Order must be at least zero.");
        CHECK(dirs.size() == values.size(),
            "Directions and values must have the same size.");

        // Solve a linear least squares system Ax = b for the coefficients, x.
        // Each row in the matrix A are the values of the spherical harmonic basis
        // functions evaluated at that sample's direction (from @dirs). The
        // corresponding row in b is the value in @values.
        std::unique_ptr<std::vector<double>> coeffs(new std::vector<double>());
        coeffs->assign(GetCoefficientCount(order), 0.0);

        DynamicMatrixf basis_values(static_cast<int>(dirs.size()), static_cast<int>(coeffs->size()));
        DynamicMatrixf func_values(static_cast<int>(dirs.size()), 1);

        double phi, theta;
        for (unsigned int i = 0; i < dirs.size(); i++) {
            func_values(i, 0) = static_cast<float>(values[i]);
            ToSphericalCoords(dirs[i], &phi, &theta);

            for (int l = 0; l <= order; l++) {
                for (int m = -l; m <= l; m++) {
                    basis_values(i, GetIndex(l, m)) = static_cast<float>(EvalSH(l, m, phi, theta));
                }
            }
        }

        // Use SVD to find the least squares fit for the coefficients of the basis
        // functions that best match the data
        DynamicMatrixf soln(static_cast<int>(coeffs->size()), 1);
        leastSquares(basis_values, func_values, soln);

        // Copy everything over to our coeffs array
        for (unsigned int i = 0; i < coeffs->size(); i++) {
            (*coeffs)[i] = soln(i, 0);
        }
        return coeffs;
    }

    void ProjectSparseSamples(const int order, const std::vector<Vector3d>& dirs,
        const std::vector<double>& values, std::vector<double>& coeffs) {
        CHECK(order >= 0, "Order must be at least zero.");
        CHECK(dirs.size() == values.size(),
            "Directions and values must have the same size.");

        // Solve a linear least squares system Ax = b for the coefficients, x.
        // Each row in the matrix A are the values of the spherical harmonic basis
        // functions evaluated at that sample's direction (from @dirs). The
        // corresponding row in b is the value in @values.
        coeffs.assign(GetCoefficientCount(order), 0.0);

        DynamicMatrixf basis_values(static_cast<int>(dirs.size()), static_cast<int>(coeffs.size()));
        DynamicMatrixf func_values(static_cast<int>(dirs.size()), 1);

        double phi, theta;
        for (unsigned int i = 0; i < dirs.size(); i++) {
            func_values(i, 0) = static_cast<float>(values[i]);
            ToSphericalCoords(dirs[i], &phi, &theta);

            for (int l = 0; l <= order; l++) {
                for (int m = -l; m <= l; m++) {
                    basis_values(i, GetIndex(l, m)) = static_cast<float>(EvalSH(l, m, phi, theta));
                }
            }
        }

        // Use SVD to find the least squares fit for the coefficients of the basis
        // functions that best match the data
        DynamicMatrixf soln(static_cast<int>(coeffs.size()), 1);
        leastSquares(basis_values, func_values, soln);

        // Copy everything over to our coeffs array
        for (unsigned int i = 0; i < coeffs.size(); i++) {
            coeffs[i] = soln(i, 0);
        }
    }
#endif

    template <typename T>
    T EvalSHSum(const int order, const T* coeffs, const double phi, const double theta) {
        if (order <= kHardCodedOrderLimit) {
            // It is faster to compute the cartesian coordinates once
            return EvalSHSum(order, coeffs, ToVector(phi, theta));
        }

        CHECK(GetCoefficientCount(order) == coeffs.size(),
            "Incorrect number of coefficients provided.");
        T sum = Zero<T>();
        for (int l = 0; l <= order; l++) {
            for (int m = -l; m <= l; m++) {
                sum += static_cast<float>(EvalSH(l, m, phi, theta)) * coeffs[GetIndex(l, m)];
            }
        }
        return sum;
    }

    template <typename T>
    T EvalSHSum(const int order, const T* coeffs,
        const Vector3d& dir) {
        if (order > kHardCodedOrderLimit) {
            // It is faster to switch to spherical coordinates
            double phi, theta;
            ToSphericalCoords(dir, &phi, &theta);
            return EvalSHSum(order, coeffs, phi, theta);
        }

        CHECK(GetCoefficientCount(order) == coeffs.size(),
            "Incorrect number of coefficients provided.");
        CHECK(NearByMargin(dir.length(), 1.0), "dir is not unit.");

        T sum = Zero<T>();
        for (int l = 0; l <= order; l++) {
            for (int m = -l; m <= l; m++) {
                sum += static_cast<float>(EvalSH(l, m, dir)) * coeffs[GetIndex(l, m)];
            }
        }
        return sum;
    }

    template double EvalSHSum<double>(const int order, const double* coeffs,
        const double phi, const double theta);
    template double EvalSHSum<double>(const int order, const double* coeffs,
        const Vector3d& dir);

    template float EvalSHSum<float>(const int order, const float* coeffs,
        const double phi, const double theta);
    template float EvalSHSum<float>(const int order, const float* coeffs,
        const Vector3d& dir);


    Rotation::Rotation(int order)
        : 
        order_(order), 
        rotation_(Quaternionf())
    {
        for (auto l = 0; l <= order; ++l)
        {
            band_rotations_.push_back(DynamicMatrixf(2 * l + 1, 2 * l + 1));
            band_coefficients_.push_back(DynamicMatrixf(2 * l + 1, 1));
            band_coefficients_rotated_.push_back(DynamicMatrixf(2 * l + 1, 1));
        }
    }
        
    Rotation::Rotation(int order, const Rotation& rotation)
        : Rotation(order)
    {
        CHECK(order >= 0, "Order must be at least 0.");

        // Copy up to min(order, rotation.order_) band rotations into the new
        // SHRotation. For shared orders, they are the same. If the new order is
        // higher than already calculated then the remainder will be computed next.
        for (int l = 0; l <= std::min(order, rotation.order_); l++)
            band_rotations_[l] = rotation.band_rotations_[l];

        // Calculate remaining bands (automatically skipped if there are no more).
        for (int l = rotation.order_ + 1; l <= order; l++) 
            ComputeBandRotation(l, &band_rotations_);
    }

    void Rotation::SetRotation(const Quaternionf& quaternion)
    {
        SetRotation(quaternion.toRotationMatrix());
    }

    void Rotation::SetRotation(const CoordinateSpace3f& orientation)
    {
        Matrix3x3f rotationMatrix{};

        rotationMatrix(0, 0) = orientation.ahead.x();
        rotationMatrix(0, 1) = orientation.ahead.y();
        rotationMatrix(0, 2) = orientation.ahead.z();
        rotationMatrix(1, 0) = -orientation.right.x();
        rotationMatrix(1, 1) = -orientation.right.y();
        rotationMatrix(1, 2) = -orientation.right.z();
        rotationMatrix(2, 0) = orientation.up.x();
        rotationMatrix(2, 1) = orientation.up.y();
        rotationMatrix(2, 2) = orientation.up.z();

        SetRotation(rotationMatrix);
    }

    void Rotation::SetRotation(const Matrix3x3f& rotation)
    {
        // Order 0 (first band) is simply the 1x1 identity since the SH basis
        // function is a simple sphere.
        band_rotations_[0](0, 0) = 1.0f;

        // The second band's transformation is simply a permutation of the
        // rotation matrix's elements, provided in Appendix 1 of [1].
        //
        // NOTE: Condon-Shortley phase has been removed from the definition of
        //       the band 1 SH rotation matrix.
        if (order_ > 0)
        {
            band_rotations_[1](0, 0) = rotation(1, 1);
            band_rotations_[1](0, 1) = rotation(1, 2);
            band_rotations_[1](0, 2) = rotation(1, 0);
            band_rotations_[1](1, 0) = rotation(2, 1);
            band_rotations_[1](1, 1) = rotation(2, 2);
            band_rotations_[1](1, 2) = rotation(2, 0);
            band_rotations_[1](2, 0) = rotation(0, 1);
            band_rotations_[1](2, 1) = rotation(0, 2);
            band_rotations_[1](2, 2) = rotation(0, 0);
        }

        // Recursively build the remaining band rotations, using the equations
        // provided in [4, 4b].
        for (int l = 2; l <= order_; l++)
            ComputeBandRotation(l, &band_rotations_);
    }

    int Rotation::order() const 
    { 
        return order_; 
    }

    Quaternionf Rotation::rotation() const 
    { 
        return rotation_; 
    }

    const DynamicMatrixf& Rotation::band_rotation(int l) const 
    {
        return band_rotations_[l];
    }

    float& Rotation::coefficient(const int l, const int m)
    {
        return band_coefficients_[l](m + l, 0);
    }

    float& Rotation::rotatedCoefficient(const int l, const int m)
    {
        return band_coefficients_rotated_[l](m + l, 0);
    }

    void Rotation::Apply(const int order, const float* coeff, float* result) const
    {
        CHECK(coeff.size() == GetCoefficientCount(order_), "Incorrect number of coefficients provided.");

        // Because of orthogonality, the coefficients outside of each band do not
        // interact with one another. By separating them into band-specific matrices,
        // we take advantage of that sparsity.
        auto offset = 0;
        for (int l = 0; l <= std::min(order, order_); l++)
        {
            multiplyMatrixVector(band_rotations_[l], &coeff[offset], &result[offset]);
            offset += 2 * l + 1;
        }
    }

    void Rotation::Apply(const int order)
    {
        // Because of orthogonality, the coefficients outside of each band do not
        // interact with one another. By separating them into band-specific matrices,
        // we take advantage of that sparsity.
        band_coefficients_rotated_[0] = band_coefficients_[0];
        for (int l = 1; l <= std::min(order, order_); l++)
            multiplyMatrices(band_rotations_[l], band_coefficients_[l], band_coefficients_rotated_[l]);
    }

} // namespace sh
