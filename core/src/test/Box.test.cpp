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

#include <box.h>

TEST_CASE("Box is sized correctly.", "[Box]")
{
    REQUIRE(sizeof(ipl::Box) == 32);
}

TEST_CASE("Box center is computed correctly.", "[box]")
{
    ipl::Box box(ipl::Vector3f(0.0f, 0.0f, 0.0f), ipl::Vector3f(1.0f, 1.0f, 1.0f));
    auto center = box.center();

    REQUIRE(center.x() == Approx(0.5));
    REQUIRE(center.y() == Approx(0.5));
    REQUIRE(center.z() == Approx(0.5));
}

TEST_CASE("Box extents are calculated correctly.", "[box]")
{
    ipl::Box box(ipl::Vector3f(0.1f, 0.1f, 0.1f), ipl::Vector3f(1.0f, 1.0f, 1.0f));
    auto extents = box.extents();

    REQUIRE(extents.x() == Approx(0.9));
    REQUIRE(extents.y() == Approx(0.9));
    REQUIRE(extents.z() == Approx(0.9));
}

TEST_CASE("Box surface area is calculated correctly.", "[box]")
{
    ipl::Box box(ipl::Vector3f(0.0f, 0.0f, 0.0f), ipl::Vector3f(1.0f, 1.0f, 1.0f));
    auto surfaceArea = box.surfaceArea();

    REQUIRE(surfaceArea == Approx(6.0));
}
