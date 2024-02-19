//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <catch.hpp>

#include <polar_vector.h>

TEST_CASE("SphericalVector converts to cartesian", "[SphericalVector]")
{
    // The convention is:
    //
    // ahead = -z
    // up = +y
    // right = +x


    // A spherical vector with radius = 1, elevation = 0, and azimuth = 0
    // should be pointing along the "ahead" vector (0, 0, -1).
    SECTION("ahead")
    {
        ipl::SphericalVector3f testVector(1, 0, 0);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(0.0));
        REQUIRE(resultVector.y() == Approx(0.0));
        REQUIRE(resultVector.z() == Approx(-1.0));
    }

    // A spherical vector with radius = 1, elevation = 0, and azimuth = Pi
    // should be pointing along the "behind" vector (0, 0, 1).
    SECTION("behind")
    {
        ipl::SphericalVector3f testVector(1, 0, ipl::Math::kPi);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(0.0));
        REQUIRE(resultVector.y() == Approx(0.0));
        REQUIRE(resultVector.z() == Approx(1.0));
    }

    // A spherical vector with radius = 1, elevation = Pi/2, and azimuth = 0
    // should be pointing along the "up" vector (0, 1, 0).
    SECTION("up")
    {
        ipl::SphericalVector3f testVector(1, ipl::Math::kHalfPi, 0);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(0.0));
        REQUIRE(resultVector.y() == Approx(1.0));
        REQUIRE(resultVector.z() == Approx(0.0));
    }

    // A spherical vector with radius = 1, elevation = -Pi/2, and azimuth = 0
    // should be pointing along the "down" vector (0, -1, 0).
    SECTION("down")
    {
        ipl::SphericalVector3f testVector(1, -ipl::Math::kHalfPi, 0);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(0.0));
        REQUIRE(resultVector.y() == Approx(-1.0));
        REQUIRE(resultVector.z() == Approx(0.0));
    }

    // A spherical vector with radius = 1, elevation = 0, and azimuth = 3Pi/2
    // should be pointing along the "right" vector (1, 0, 0).
    SECTION("right")
    {
        ipl::SphericalVector3f testVector(1, 0, 3 * ipl::Math::kHalfPi);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(1.0));
        REQUIRE(resultVector.y() == Approx(0.0));
        REQUIRE(resultVector.z() == Approx(0.0));
    }

    // A spherical vector with radius = 1, elevation = 0, and azimuth = Pi/2
    // should be pointing along the "left" vector (-1, 0, 0).
    SECTION("left")
    {
        ipl::SphericalVector3f testVector(1, 0, ipl::Math::kHalfPi);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(-1.0));
        REQUIRE(resultVector.y() == Approx(0.0));
        REQUIRE(resultVector.z() == Approx(0.0));
    }
}

TEST_CASE("SphericalVector converts from cartesian", "[SphericalVector]")
{
    // The convention is:
    //
    // ahead = -z
    // up = +y
    // right = +x

    // A cartesian vector pointing along the "ahead" vector (0, 0, -1)
    // should produce a spherical vector with radius = 1, elevation = 0, and azimuth = 0.
    SECTION("ahead")
    {
        ipl::Vector3f cartesianVector(0, 0, -1);
        ipl::SphericalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.azimuth == Approx(0.0));
        REQUIRE(testVector.elevation == Approx(0.0));
    }

    // A cartesian vector pointing along the "behind" vector (0, 0, 1)
    // should produce a spherical vector with radius = 1, elevation = 0, and azimuth = Pi.
    SECTION("behind")
    {
        ipl::Vector3f cartesianVector(0, 0, 1);
        ipl::SphericalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.azimuth == Approx(ipl::Math::kPi));
        REQUIRE(testVector.elevation == Approx(0.0));
    }

    // A cartesian vector pointing along the "up" vector (0, 1, 0)
    // should produce a spherical vector with radius = 1, elevation = Pi/2,
    // and azimuth can be whatever.
    SECTION("up")
    {
        ipl::Vector3f cartesianVector(0, 1, 0);
        ipl::SphericalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.elevation == Approx(ipl::Math::kHalfPi));
    }

    // A cartesian vector pointing along the "down" vector (0, -1, 0)
    // should produce a spherical vector with radius = 1, elevation = -Pi/2,
    // and azimuth can be whatever.
    SECTION("down")
    {
        ipl::Vector3f cartesianVector(0, -1, 0);
        ipl::SphericalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.elevation == Approx(-ipl::Math::kHalfPi));
    }

    // A cartesian vector pointing along the "right" vector (1, 0, 0)
    // should produce a spherical vector with radius = 1, elevation = 0, and azimuth = 3Pi/2.
    SECTION("right")
    {
        ipl::Vector3f cartesianVector(1, 0, 0);
        ipl::SphericalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.azimuth == Approx(3 * ipl::Math::kHalfPi));
        REQUIRE(testVector.elevation == Approx(0.0));
    }

    // A cartesian vector pointing along the "left" vector (-1, 0, 0)
    // should produce a spherical vector with radius = 1, elevation = 0, and azimuth = Pi/2.
    SECTION("left")
    {
        ipl::Vector3f cartesianVector(-1, 0, 0);
        ipl::SphericalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.azimuth == Approx(ipl::Math::kHalfPi));
        REQUIRE(testVector.elevation == Approx(0.0));
    }
}

TEST_CASE("CylindricalVector converts to cartesian", "[CylindricalVector3]")
{
    // A cylindrical vector with radius = 1, height = 0, and azimuth = 0
    // should produce a cartesian vector pointing along the "ahead" vector (0, 0, -1).
    SECTION("ahead")
    {
        ipl::CylindricalVector3f testVector(1, 0, 0);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(0.0));
        REQUIRE(resultVector.y() == Approx(0.0));
        REQUIRE(resultVector.z() == Approx(-1.0));
    }

    // A cylindrical vector with radius = 1, height = 0, and azimuth = Pi
    // should produce a cartesian vector pointing along the "behind" vector (0, 0, 1).
    SECTION("behind")
    {
        ipl::CylindricalVector3f testVector(1, 0, ipl::Math::kPi);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(0.0));
        REQUIRE(resultVector.y() == Approx(0.0));
        REQUIRE(resultVector.z() == Approx(1.0));
    }

    // A cylindrical vector with radius = 0, height = 1, and azimuth = 0
    // should produce a cartesian vector pointing along the "up" vector (0, 1, 0).
    SECTION("up")
    {
        ipl::CylindricalVector3f testVector(0, 1, 0);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(0.0));
        REQUIRE(resultVector.y() == Approx(1.0));
        REQUIRE(resultVector.z() == Approx(0.0));
    }

    // A cylindrical vector with radius = 0, height = -1, and azimuth = 0
    // should produce a cartesian vector pointing along the "down" vector (0, -1, 0).
    SECTION("down")
    {
        ipl::CylindricalVector3f testVector(0, -1, 0);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(0.0));
        REQUIRE(resultVector.y() == Approx(-1.0));
        REQUIRE(resultVector.z() == Approx(0.0));
    }

    // A cylindrical vector with radius = 1, height = 0, and azimuth = 3Pi/2
    // should produce a cartesian vector pointing along the "right" vector (1, 0, 0).
    SECTION("right")
    {
        ipl::CylindricalVector3f testVector(1, 0, 3 * ipl::Math::kHalfPi);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(1.0));
        REQUIRE(resultVector.y() == Approx(0.0));
        REQUIRE(resultVector.z() == Approx(0.0));
    }

    // A cylindrical vector with radius = 1, height = 0, and azimuth = Pi/2
    // should produce a cartesian vector pointing along the "left" vector (-1, 0, 0).
    SECTION("right")
    {
        ipl::CylindricalVector3f testVector(1, 0, ipl::Math::kHalfPi);

        auto resultVector = testVector.toCartesian();

        REQUIRE(resultVector.x() == Approx(-1.0));
        REQUIRE(resultVector.y() == Approx(0.0));
        REQUIRE(resultVector.z() == Approx(0.0));
    }
}

TEST_CASE("CylindricalVector converts from cartesian", "[CylindricalVector3]")
{
    // A cartesian vector pointing along the "ahead" vector (0, 0, -1)
    // should produce a cylindrical vector with radius = 1, height = 0, and azimuth = 0.
    SECTION("ahead")
    {
        ipl::Vector3f cartesianVector(0, 0, -1);
        ipl::CylindricalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.height == Approx(0.0));
        REQUIRE(testVector.azimuth == Approx(0.0));
    }

    // A cartesian vector pointing along the "behind" vector (0, 0, 1)
    // should produce a cylindrical vector with radius = 1, height = 0, and azimuth = Pi.
    SECTION("behind")
    {
        ipl::Vector3f cartesianVector(0, 0, 1);
        ipl::CylindricalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.height == Approx(0.0));
        REQUIRE(testVector.azimuth == Approx(ipl::Math::kPi));
    }

    // A cartesian vector pointing along the "up" vector (0, 1, 0)
    // should produce a cylindrical vector with radius = 0 and height = 1.
    SECTION("up")
    {
        ipl::Vector3f cartesianVector(0, 1, 0);
        ipl::CylindricalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(0.0));
        REQUIRE(testVector.height == Approx(1.0));
    }

    // A cartesian vector pointing along the "down" vector (0, -1, 0)
    // should produce a cylindrical vector with radius = 0 and height = -1.
    SECTION("down")
    {
        ipl::Vector3f cartesianVector(0, -1, 0);
        ipl::CylindricalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(0.0));
        REQUIRE(testVector.height == Approx(-1.0));
    }

    // A cartesian vector pointing along the "right" vector (1, 0, 0)
    // should produce a cylindrical vector with radius = 1, height = 0, and azimuth = 3Pi/2.
    SECTION("right")
    {
        ipl::Vector3f cartesianVector(1, 0, 0);
        ipl::CylindricalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.height == Approx(0.0));
        REQUIRE(testVector.azimuth == Approx(3 * ipl::Math::kHalfPi));
    }

    // A cartesian vector pointing along the "left" vector (-1, 0, 0)
    // should produce a cylindrical vector with radius = 1, height = 0, and azimuth = Pi/2.
    SECTION("left")
    {
        ipl::Vector3f cartesianVector(-1, 0, 0);
        ipl::CylindricalVector3f testVector(cartesianVector);

        REQUIRE(testVector.radius == Approx(1.0));
        REQUIRE(testVector.height == Approx(0.0));
        REQUIRE(testVector.azimuth == Approx(ipl::Math::kHalfPi));
    }
}

TEST_CASE("Cartesian to Interaural", "[InterauralSphericalVector3]")
{
    SECTION("ahead")
    {
        ipl::Vector3f cartesian{ 0, 0, -1 };
        ipl::InterauralSphericalVector3f interaural{ cartesian };

        REQUIRE(interaural.radius == Approx(1.0));
        REQUIRE(interaural.azimuth == Approx(0.0));
        REQUIRE(interaural.elevation == Approx(ipl::Math::kHalfPi));
    }

    SECTION("behind")
    {
        ipl::Vector3f cartesian{ 0, 0, 1 };
        ipl::InterauralSphericalVector3f interaural{ cartesian };

        REQUIRE(interaural.radius == Approx(1.0));
        REQUIRE(interaural.azimuth == Approx(0.0));
        REQUIRE(interaural.elevation == Approx(3 * ipl::Math::kHalfPi));
    }

    SECTION("up")
    {
        ipl::Vector3f cartesian{ 0, 1, 0 };
        ipl::InterauralSphericalVector3f interaural{ cartesian };

        REQUIRE(interaural.radius == Approx(1.0));
        REQUIRE(interaural.azimuth == Approx(0.0));
        REQUIRE(interaural.elevation == Approx(ipl::Math::kPi));
    }

    SECTION("down")
    {
        ipl::Vector3f cartesian{ 0, -1, 0 };
        ipl::InterauralSphericalVector3f interaural{ cartesian };

        REQUIRE(interaural.radius == Approx(1.0));
        REQUIRE(interaural.azimuth == Approx(0.0));
        REQUIRE(interaural.elevation == Approx(0.0));
    }

    SECTION("left")
    {
        ipl::Vector3f cartesian{ -1, 0, 0 };
        ipl::InterauralSphericalVector3f interaural{ cartesian };

        REQUIRE(interaural.radius == Approx(1.0));
        REQUIRE(interaural.azimuth == Approx(-ipl::Math::kHalfPi));
        REQUIRE(interaural.elevation == Approx(0.0));
    }

    SECTION("right")
    {
        ipl::Vector3f cartesian{ 1, 0, 0 };
        ipl::InterauralSphericalVector3f interaural{ cartesian };

        REQUIRE(interaural.radius == Approx(1.0));
        REQUIRE(interaural.azimuth == Approx(ipl::Math::kHalfPi));
        REQUIRE(interaural.elevation == Approx(0.0));
    }
}

TEST_CASE("Interaural to Cartesian", "[InterauralSphericalVector3]")
{
    SECTION("ahead")
    {
        ipl::InterauralSphericalVector3f interaural{ 1, 0, ipl::Math::kHalfPi };
        ipl::Vector3f cartesian{ interaural.toCartesian() };

        REQUIRE(cartesian.x() == Approx(0.0));
        REQUIRE(cartesian.y() == Approx(0.0));
        REQUIRE(cartesian.z() == Approx(-1.0));
    }

    SECTION("behind")
    {
        ipl::InterauralSphericalVector3f interaural{ 1, 0, 3 * ipl::Math::kHalfPi };
        ipl::Vector3f cartesian{ interaural.toCartesian() };

        REQUIRE(cartesian.x() == Approx(0.0));
        REQUIRE(cartesian.y() == Approx(0.0));
        REQUIRE(cartesian.z() == Approx(1.0));
    }

    SECTION("up")
    {
        ipl::InterauralSphericalVector3f interaural{ 1, 0, ipl::Math::kPi };
        ipl::Vector3f cartesian{ interaural.toCartesian() };

        REQUIRE(cartesian.x() == Approx(0.0));
        REQUIRE(cartesian.y() == Approx(1.0));
        REQUIRE(cartesian.z() == Approx(0.0));
    }

    SECTION("down")
    {
        ipl::InterauralSphericalVector3f interaural{ 1, 0, 0 };
        ipl::Vector3f cartesian{ interaural.toCartesian() };

        REQUIRE(cartesian.x() == Approx(0.0));
        REQUIRE(cartesian.y() == Approx(-1.0));
        REQUIRE(cartesian.z() == Approx(0.0));
    }

    SECTION("left")
    {
        ipl::InterauralSphericalVector3f interaural{ 1, -ipl::Math::kHalfPi, 0 };
        ipl::Vector3f cartesian{ interaural.toCartesian() };

        REQUIRE(cartesian.x() == Approx(-1.0));
        REQUIRE(cartesian.y() == Approx(0.0));
        REQUIRE(cartesian.z() == Approx(0.0));
    }

    SECTION("right")
    {
        ipl::InterauralSphericalVector3f interaural{ 1, ipl::Math::kHalfPi, 0 };
        ipl::Vector3f cartesian{ interaural.toCartesian() };

        REQUIRE(cartesian.x() == Approx(1.0));
        REQUIRE(cartesian.y() == Approx(0.0));
        REQUIRE(cartesian.z() == Approx(0.0));
    }
}