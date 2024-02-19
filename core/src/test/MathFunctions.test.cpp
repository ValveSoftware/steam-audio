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

#include <math_functions.h>

TEST_CASE("Factorial", "[MathFunctions]")
{
    REQUIRE(ipl::Math::factorial(0) == 1);
    REQUIRE(ipl::Math::factorial(1) == 1);
    REQUIRE(ipl::Math::factorial(2) == 2);
    REQUIRE(ipl::Math::factorial(3) == 6);
    REQUIRE(ipl::Math::factorial(4) == 24);
    REQUIRE(ipl::Math::factorial(5) == 120);
    REQUIRE(ipl::Math::factorial(6) == 720);
    REQUIRE(ipl::Math::factorial(7) == 5040);
    REQUIRE(ipl::Math::factorial(8) == 40320);
}

TEST_CASE("DoubleFactorial", "[MathFunctions]")
{
    REQUIRE(ipl::Math::doubleFactorial(0) == 1);
    REQUIRE(ipl::Math::doubleFactorial(1) == 1);
    REQUIRE(ipl::Math::doubleFactorial(2) == 2);
    REQUIRE(ipl::Math::doubleFactorial(3) == 3);
    REQUIRE(ipl::Math::doubleFactorial(4) == 8);
    REQUIRE(ipl::Math::doubleFactorial(5) == 15);
    REQUIRE(ipl::Math::doubleFactorial(6) == 48);
    REQUIRE(ipl::Math::doubleFactorial(7) == 105);
    REQUIRE(ipl::Math::doubleFactorial(8) == 384);
}
