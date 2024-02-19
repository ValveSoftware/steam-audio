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

#include <coordinate_space.h>

TEST_CASE("CoordinateSpace3 produces a right-handed system", "[CoordinateSpace3]")
{
    ipl::CoordinateSpace3f testSpace(ipl::Vector3f(0, 0, -1), ipl::Vector3f(0, 1, 0), ipl::Vector3f());

    REQUIRE(testSpace.right == ipl::Vector3f(1, 0, 0));
}

TEST_CASE("CoordinateSpace3 produces an orthonormal basis", "[CoordinateSpace3]")
{
    ipl::CoordinateSpace3f testSpace(ipl::Vector3f(1, 0, 0), ipl::Vector3f());

    REQUIRE(ipl::Vector3f::dot(testSpace.ahead, testSpace.right) == Approx(0));
    REQUIRE(ipl::Vector3f::dot(testSpace.ahead, testSpace.up) == Approx(0));
    REQUIRE(ipl::Vector3f::dot(testSpace.up, testSpace.right) == Approx(0));
}

TEST_CASE("CoordinateSpace3 transforms local to world correctly", "[CoordinateSpace3]")
{
    auto testVec = ipl::Vector3f::unitVector(ipl::Vector3f(-3.0f, 5.0f, 6.0f));
    ipl::CoordinateSpace3f testSpace(testVec, ipl::Vector3f::kZero);

    auto transformedAhead = testSpace.transformDirectionFromLocalToWorld(-ipl::Vector3f::kZAxis);
    REQUIRE(transformedAhead.x() == Approx(testVec.x()));
    REQUIRE(transformedAhead.y() == Approx(testVec.y()));
    REQUIRE(transformedAhead.z() == Approx(testVec.z()));

    ipl::CoordinateSpace3f testSpaceX(ipl::Vector3f::kXAxis, ipl::Vector3f::kZero);

    auto transformedX = testSpaceX.transformDirectionFromLocalToWorld(-ipl::Vector3f::kZAxis);
    REQUIRE(transformedX.x() == Approx(ipl::Vector3f::kXAxis.x()));
    REQUIRE(transformedX.y() == Approx(ipl::Vector3f::kXAxis.y()));
    REQUIRE(transformedX.z() == Approx(ipl::Vector3f::kXAxis.z()));

    ipl::CoordinateSpace3f testSpaceY(ipl::Vector3f::kYAxis, ipl::Vector3f::kZero);

    auto transformedY = testSpaceY.transformDirectionFromLocalToWorld(-ipl::Vector3f::kZAxis);
    REQUIRE(transformedY.x() == Approx(ipl::Vector3f::kYAxis.x()));
    REQUIRE(transformedY.y() == Approx(ipl::Vector3f::kYAxis.y()));
    REQUIRE(transformedY.z() == Approx(ipl::Vector3f::kYAxis.z()));

    ipl::CoordinateSpace3f testSpaceZ(ipl::Vector3f::kZAxis, ipl::Vector3f::kZero);

    auto transformedZ = testSpaceZ.transformDirectionFromLocalToWorld(-ipl::Vector3f::kZAxis);
    REQUIRE(transformedZ.x() == Approx(ipl::Vector3f::kZAxis.x()));
    REQUIRE(transformedZ.y() == Approx(ipl::Vector3f::kZAxis.y()));
    REQUIRE(transformedZ.z() == Approx(ipl::Vector3f::kZAxis.z()));
}
