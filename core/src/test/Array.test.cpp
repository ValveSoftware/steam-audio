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

#include <array.h>
#include <array_math.h>


TEST_CASE("Array is created with the specified size", "[Array]")
{
    ipl::Array<int> a(10);
    REQUIRE(a.size(0) == 10);
}

TEST_CASE("Array elements can be accessed correctly", "[Array]")
{
    ipl::Array<int> a(10);
    a[0] = 12;
    a[1] = 157;

    REQUIRE(a[0] == 12);
    REQUIRE(a[1] == 157);
    REQUIRE(a.data()[0] == a[0]);
    REQUIRE(a.data()[1] == a[1]);
}

TEST_CASE("ArrayMath::multiplyAccumulate works on unaligned complex arrays", "[ArrayMath]")
{
    // one element past 16-byte aligned storage: every pointer is only 8-byte aligned
    alignas(16) ipl::complex_t in1[5], in2[5], accum[5];
    for (auto i = 0; i < 5; ++i)
    {
        in1[i] = ipl::complex_t(1.0f, 2.0f);
        in2[i] = ipl::complex_t(3.0f, 4.0f);
        accum[i] = ipl::complex_t(0.0f, 0.0f);
    }

    ipl::ArrayMath::multiplyAccumulate(4, &in1[1], &in2[1], &accum[1]);

    for (auto i = 1; i < 5; ++i)
    {
        REQUIRE(accum[i].real() == -5.0f);
        REQUIRE(accum[i].imag() == 10.0f);
    }
}
