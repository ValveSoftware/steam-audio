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

#include <ray.h>

TEST_CASE("Evaluating a point using the parametric equation works.", "[Ray]")
{
    ipl::Ray ray{ ipl::Vector3f(1.0f, 4.0f, 6.0f), ipl::Vector3f::unitVector(ipl::Vector3f(-3.0f, 4.0f, 0.0f)) };
    ipl::Vector3f point = ray.pointAtDistance(10.0f);

    REQUIRE(point.x() == Approx(-5.0));
    REQUIRE(point.y() == Approx(12.0));
    REQUIRE(point.z() == Approx(6.0));
}
