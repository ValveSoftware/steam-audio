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

#include <bvh.h>

TEST_CASE("BvhNode", "[BvhNode]")
{
}

TEST_CASE("GrowableBox grows to contain a point correctly", "[GrowableBox]")
{
    alignas(ipl::Memory::kDefaultAlignment)ipl::GrowableBox growableBox;
    alignas(ipl::Memory::kDefaultAlignment)ipl::Vector3f test(1.0f, 2.0f, 3.0f);
    growableBox.growToContain(test);

    alignas(ipl::Memory::kDefaultAlignment)ipl::Box box;
    growableBox.store(box);

    REQUIRE(box.coordinates(0).x() == Approx(1.0));
    REQUIRE(box.coordinates(0).y() == Approx(2.0));
    REQUIRE(box.coordinates(0).z() == Approx(3.0));
    REQUIRE(box.coordinates(1).x() == Approx(1.0));
    REQUIRE(box.coordinates(1).y() == Approx(2.0));
    REQUIRE(box.coordinates(1).z() == Approx(3.0));
}

TEST_CASE("GrowableBox grows to contain a box correctly", "[GrowableBox]")
{
    alignas(ipl::Memory::kDefaultAlignment)ipl::GrowableBox growableBox;
    alignas(ipl::Memory::kDefaultAlignment)ipl::Vector3f t1(1.0f, 2.0f, 3.0f);
    growableBox.growToContain(t1);

    alignas(ipl::Memory::kDefaultAlignment)ipl::GrowableBox growableBox2;
    alignas(ipl::Memory::kDefaultAlignment)ipl::Vector3f t2(10.0f, 12.0f, -32.0f);
    growableBox2.growToContain(t2);

    growableBox.growToContain(growableBox2);

    alignas(ipl::Memory::kDefaultAlignment)ipl::Box box;
    growableBox.store(box);

    REQUIRE(box.coordinates(0).x() == Approx(1.0));
    REQUIRE(box.coordinates(0).y() == Approx(2.0));
    REQUIRE(box.coordinates(0).z() == Approx(-32.0));
    REQUIRE(box.coordinates(1).x() == Approx(10.0));
    REQUIRE(box.coordinates(1).y() == Approx(12.0));
    REQUIRE(box.coordinates(1).z() == Approx(3.0));
}

TEST_CASE("GrowableBox surface area is calculated correctly", "[GrowableBox]")
{
    alignas(ipl::Memory::kDefaultAlignment)ipl::GrowableBox growableBox;
    alignas(ipl::Memory::kDefaultAlignment)ipl::Vector3f t1(1.0f, 2.0f, 3.0f);
    growableBox.growToContain(t1);

    alignas(ipl::Memory::kDefaultAlignment)ipl::GrowableBox growableBox2;
    alignas(ipl::Memory::kDefaultAlignment)ipl::Vector3f t2(10.0f, 12.0f, -32.0f);
    growableBox2.growToContain(t2);

    growableBox.growToContain(growableBox2);

    REQUIRE(growableBox.getSurfaceArea() == Approx(1510.0));
}

TEST_CASE("Split", "[Split]")
{
}

TEST_CASE("CentroidCoordinate", "[CentroidCoordinate]")
{
}

TEST_CASE("ConstructionTask", "[ConstructionTask]")
{
}

TEST_CASE("TraversalTask", "[TraversalTask]")
{
}

TEST_CASE("Bvh", "[Bvh]")
{
}
