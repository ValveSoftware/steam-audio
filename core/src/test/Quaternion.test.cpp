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

#include <quaternion.h>

TEST_CASE("Quaternion * Quaternion", "[Quaternion]")
{
    ipl::Quaterniond a(2, 3, 4, 5);
    ipl::Quaterniond b(6, 7, 8, 9);

    auto c = a * b;

    REQUIRE(c.x == Approx(44.0));
    REQUIRE(c.y == Approx(70.0));
    REQUIRE(c.z == Approx(72.0));
    REQUIRE(c.w == Approx(-20.0));
}

TEST_CASE("Quaternion normalize", "[Quaternion]")
{
    ipl::Quaterniond q(1, 2, 3, 4);
    q.normalize();

    REQUIRE(q.x == Approx(0.18257418583505536));
    REQUIRE(q.y == Approx(0.36514837167011072));
    REQUIRE(q.z == Approx(0.54772255750516607));
    REQUIRE(q.w == Approx(0.73029674334022143));
}

TEST_CASE("Quaternion::toRotationMatrix", "[Quaternion]")
{
    ipl::Quaterniond q(1, 2, 3, 4);

    q.normalize();
    auto rotMat = q.toRotationMatrix();

    REQUIRE(rotMat(0, 0) == Approx(0.13333));
    REQUIRE(rotMat(0, 1) == Approx(-0.66666));
    REQUIRE(rotMat(0, 2) == Approx(0.73333));

    REQUIRE(rotMat(1, 0) == Approx(0.93333));
    REQUIRE(rotMat(1, 1) == Approx(0.33333));
    REQUIRE(rotMat(1, 2) == Approx(0.13333));

    REQUIRE(rotMat(2, 0) == Approx(-0.33333));
    REQUIRE(rotMat(2, 1) == Approx(0.66667));
    REQUIRE(rotMat(2, 2) == Approx(0.66667));
}
