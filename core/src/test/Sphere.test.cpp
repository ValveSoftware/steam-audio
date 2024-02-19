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

#include <sphere.h>

//============================================================================
// Sphere
//============================================================================

TEST_CASE("Sphere is sized correctly.", "[Sphere]")
{
    REQUIRE(sizeof(ipl::Sphere) == 16);
}

TEST_CASE("Sphere::contains correctly decides whether a point is within the sphere", "[Sphere]")
{
    ipl::Sphere sA;
    sA.center = ipl::Vector3f(4, 4, 4);
    sA.radius = 4;

    ipl::Vector3f inA(4, 4, 4);
    ipl::Vector3f inB(5, 5, 5);
    ipl::Vector3f inC(7.5, 4, 4);

    ipl::Vector3f outA(10, 4, 4);
    ipl::Vector3f outB(-1, -1, -1);
    ipl::Vector3f outC(0, 0, 0);

    REQUIRE(sA.contains(inA));
    REQUIRE(sA.contains(inB));
    REQUIRE(sA.contains(inC));
    REQUIRE_FALSE(sA.contains(outA));
    REQUIRE_FALSE(sA.contains(outB));
    REQUIRE_FALSE(sA.contains(outC));
}

TEST_CASE("BoundingSphere correctly computes bounding sphere for sphere-inside-sphere", "[Sphere]")
{
    ipl::Sphere sA;
    sA.center = ipl::Vector3f(2, 2, 2);
    sA.radius = 10;

    ipl::Sphere sB;
    sB.center = ipl::Vector3f(3, 3, 3);
    sB.radius = 4;

    auto boundingSphere = ipl::computeBoundingSphere(sA, sB);

    REQUIRE(boundingSphere.center == sA.center);
    REQUIRE(boundingSphere.radius == sA.radius);
}

TEST_CASE("computeBoundingSphere correctly computes bounding sphere for disjoint spheres", "[Sphere]")
{
    ipl::Sphere sA;
    sA.center = ipl::Vector3f(-2, 0, 0);
    sA.radius = 2;

    ipl::Sphere sB;
    sB.center = ipl::Vector3f(2, 0, 0);
    sB.radius = 2;

    auto boundingSphere = ipl::computeBoundingSphere(sA, sB);

    REQUIRE(boundingSphere.center.x() == Approx(0));
    REQUIRE(boundingSphere.center.y() == Approx(0));
    REQUIRE(boundingSphere.center.z() == Approx(0));
    REQUIRE(boundingSphere.radius == Approx(4));
}
