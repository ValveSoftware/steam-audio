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

#include <vector.h>

TEST_CASE("Vector classes have correct sizes.", "[Vector]")
{
    REQUIRE(sizeof(ipl::Vector2f) == 8);
    REQUIRE(sizeof(ipl::Vector3f) == 12);
    REQUIRE(sizeof(ipl::Vector4f) == 16);
}

TEST_CASE("Vector3 can convert into Vector4 homogeneous coordinates", "[Vector]")
{
    ipl::Vector3f testVector3f(3.0f, 4.0f, 5.0f);
    ipl::Vector4f testVector4f(testVector3f);

    REQUIRE(testVector4f.x() == 3.0f);
    REQUIRE(testVector4f.y() == 4.0f);
    REQUIRE(testVector4f.z() == 5.0f);
    REQUIRE(testVector4f.w() == 1.0f);
}

TEST_CASE("operator- returns negated vector", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    auto negated = -testVector4A_0;
    REQUIRE(negated[0] == -50);
    REQUIRE(negated[1] == 0);
    REQUIRE(negated[2] == 200);
    REQUIRE(negated[3] == 3);
}

TEST_CASE("operator< returns true if A is component-wise LT B", "[Vector]")
{
    REQUIRE((ipl::Vector<int, 3>{0, 0, 0} < ipl::Vector<int, 3>{1, 1, 1}));

    REQUIRE_FALSE((ipl::Vector<int, 3>{1, 0, 0} < ipl::Vector<int, 3>{1, 1, 1}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{0, 1, 0} < ipl::Vector<int, 3>{1, 1, 1}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{0, 0, 1} < ipl::Vector<int, 3>{1, 1, 1}));

    REQUIRE_FALSE((ipl::Vector<int, 3>{2, 0, 0} < ipl::Vector<int, 3>{1, 1, 1}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{0, 2, 0} < ipl::Vector<int, 3>{1, 1, 1}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{0, 0, 2} < ipl::Vector<int, 3>{1, 1, 1}));
}

TEST_CASE("operator<= returns true if A is component-wise LTE B", "[Vector]")
{
    REQUIRE((ipl::Vector<int, 3>{0, 0, 0} <= ipl::Vector<int, 3>{1, 1, 1}));

    REQUIRE((ipl::Vector<int, 3>{1, 0, 0} <= ipl::Vector<int, 3>{1, 1, 1}));
    REQUIRE((ipl::Vector<int, 3>{0, 1, 0} <= ipl::Vector<int, 3>{1, 1, 1}));
    REQUIRE((ipl::Vector<int, 3>{0, 0, 1} <= ipl::Vector<int, 3>{1, 1, 1}));

    REQUIRE((ipl::Vector<int, 3>{1, 1, 1} <= ipl::Vector<int, 3>{1, 1, 1}));

    REQUIRE_FALSE((ipl::Vector<int, 3>{2, 0, 0} <= ipl::Vector<int, 3>{1, 1, 1}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{0, 2, 0} <= ipl::Vector<int, 3>{1, 1, 1}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{0, 0, 2} <= ipl::Vector<int, 3>{1, 1, 1}));
}

TEST_CASE("operator> returns true if A is component-wise GT B", "[Vector]")
{
    REQUIRE((ipl::Vector<int, 3>{1, 1, 1} > ipl::Vector<int, 3>{0, 0, 0}));

    REQUIRE_FALSE((ipl::Vector<int, 3>{0, 1, 1} > ipl::Vector<int, 3>{0, 0, 0}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{1, 0, 1} > ipl::Vector<int, 3>{0, 0, 0}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{1, 1, 0} > ipl::Vector<int, 3>{0, 0, 0}));

    REQUIRE_FALSE((ipl::Vector<int, 3>{-1, 1, 1} > ipl::Vector<int, 3>{0, 0, 0}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{1, -1, 1} > ipl::Vector<int, 3>{0, 0, 0}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{1, 1, -1} > ipl::Vector<int, 3>{0, 0, 0}));
}

TEST_CASE("operator>= returns true if A is component-wise GTE B", "[Vector]")
{
    REQUIRE((ipl::Vector<int, 3>{1, 1, 1} >= ipl::Vector<int, 3>{0, 0, 0}));

    REQUIRE((ipl::Vector<int, 3>{0, 1, 1} >= ipl::Vector<int, 3>{0, 0, 0}));
    REQUIRE((ipl::Vector<int, 3>{1, 0, 1} >= ipl::Vector<int, 3>{0, 0, 0}));
    REQUIRE((ipl::Vector<int, 3>{1, 1, 0} >= ipl::Vector<int, 3>{0, 0, 0}));

    REQUIRE((ipl::Vector<int, 3>{1, 1, 1} >= ipl::Vector<int, 3>{1, 1, 1}));

    REQUIRE_FALSE((ipl::Vector<int, 3>{-1, 1, 1} >= ipl::Vector<int, 3>{0, 0, 0}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{1, -1, 1} >= ipl::Vector<int, 3>{0, 0, 0}));
    REQUIRE_FALSE((ipl::Vector<int, 3>{1, 1, -1} >= ipl::Vector<int, 3>{0, 0, 0}));
}

TEST_CASE("minComponent() returns the minimum component", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    REQUIRE(testVector4A_0.minComponent() == -200);
}

TEST_CASE("maxComponent() returns maximum component", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    REQUIRE(testVector4A_0.maxComponent() == 50);
}

TEST_CASE("minAbsComponent() returns the component with the least absolute value", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    REQUIRE(testVector4A_0.minAbsComponent() == 0);
}

TEST_CASE("maxAbsComponent() returns the component with the most absolute value", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    REQUIRE(testVector4A_0.maxAbsComponent() == -200);
}

TEST_CASE("indexOfMinComponent() returns the index of the minimum component", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    REQUIRE(testVector4A_0.indexOfMinComponent() == 2);
}

TEST_CASE("indexOfMaxComponent() returns the index of the maximum component", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    REQUIRE(testVector4A_0.indexOfMaxComponent() == 0);
}

TEST_CASE("indexOfMinAbsComponent() returns the index of the component with the least absolute value", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    REQUIRE(testVector4A_0.indexOfMinAbsComponent() == 1);
}

TEST_CASE("indexOfMaxAbsComponent() returns the index of the component with the most absolute value", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    REQUIRE(testVector4A_0.indexOfMaxAbsComponent() == 2);
}

TEST_CASE("dot(a, b) correctly computes the dot product of a and b", "[Vector]")
{
    ipl::Vector2f vA(2.0f, 4.0f);
    ipl::Vector2f vB(3.0f, 7.0f);

    auto aDotB = ipl::Vector2f::dot(vA, vB);
    REQUIRE(aDotB == 34.0f);
}

TEST_CASE("min(a, b) returns the componentwise minimum elements between a and b", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    ipl::Vector<int, 4> testVector4A_1{ 23, 7, -1500, 500 };

    auto minOf01 = ipl::Vector<int, 4>::min(testVector4A_0, testVector4A_1);
    REQUIRE(minOf01[0] == 23);
    REQUIRE(minOf01[1] == 0);
    REQUIRE(minOf01[2] == -1500);
    REQUIRE(minOf01[3] == -3);
}

TEST_CASE("max(a, b) returns the componentwise maximum elements between a and b", "[Vector]")
{
    ipl::Vector<int, 4> testVector4A_0{ 50, 0, -200, -3 };
    ipl::Vector<int, 4> testVector4A_1{ 23, 7, -1500, 500 };

    auto maxOf01 = ipl::Vector<int, 4>::max(testVector4A_0, testVector4A_1);
    REQUIRE(maxOf01[0] == 50);
    REQUIRE(maxOf01[1] == 7);
    REQUIRE(maxOf01[2] == -200);
    REQUIRE(maxOf01[3] == 500);
}

TEST_CASE("reciprocal(v) returns the componentwise reciprocal of v", "[Vector]")
{
    // TODO: test for division by zero
    ipl::Vector4f testVector4A_0{ 50, 6, -200, -3 };
    auto reciprocalOf0 = ipl::Vector4f::reciprocal(testVector4A_0);
    REQUIRE(reciprocalOf0[0] == 1.0f / 50.0f);
    REQUIRE(reciprocalOf0[1] == 1.0f / 6.0f);
    REQUIRE(reciprocalOf0[2] == 1.0f / -200.0f);
    REQUIRE(reciprocalOf0[3] == 1.0f / -3.0f);
}

TEST_CASE("sqrt(v) returns the componentwise square root of v", "[Vector]")
{
    // TODO: test for negative numbers
    ipl::Vector4f testVector4A_0{ 50, 6, 200, 3 };
    auto sqrtOf0 = ipl::Vector4f::sqrt(testVector4A_0);
    REQUIRE(sqrtOf0[0] == std::sqrt(50.0f));
    REQUIRE(sqrtOf0[1] == std::sqrt(6.0f));
    REQUIRE(sqrtOf0[2] == std::sqrt(200.0f));
    REQUIRE(sqrtOf0[3] == std::sqrt(3.0f));
}

TEST_CASE("lengthSquared() returns the squared length", "[Vector]")
{
    ipl::Vector4f testVector4A_0{ 0, 0, 5, 0 };
    REQUIRE(testVector4A_0.lengthSquared() == 25);
}

TEST_CASE("length() returns the length", "[Vector]")
{
    ipl::Vector4f testVector4A_0{ 0, 0, 5, 0 };
    REQUIRE(testVector4A_0.length() == 5);
}

TEST_CASE("Vector3 cross product is calculated correctly.", "[Vector]")
{
    auto v = ipl::Vector3f::cross(ipl::Vector3f::kXAxis, ipl::Vector3f::kYAxis);
    REQUIRE(v.x() == Approx(0.0));
    REQUIRE(v.y() == Approx(0.0));
    REQUIRE(v.z() == Approx(1.0));

    v = ipl::Vector3f::cross(ipl::Vector3f::kZAxis, ipl::Vector3f::kYAxis);
    REQUIRE(v.x() == Approx(-1.0));
    REQUIRE(v.y() == Approx(0.0));
    REQUIRE(v.z() == Approx(0.0));

    v = ipl::Vector3f::cross(ipl::Vector3f::kXAxis, ipl::Vector3f::kZAxis);
    REQUIRE(v.x() == Approx(0.0));
    REQUIRE(v.y() == Approx(-1.0));
    REQUIRE(v.z() == Approx(0.0));
}
