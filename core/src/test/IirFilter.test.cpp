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

#include <iir.h>
#include <array.h>

TEST_CASE("IIR filter is applied correctly.", "[IirFilter]")
{
    auto filter = ipl::IIR{};
    auto filterer = ipl::IIRFilterer{};
    auto coefficients = reinterpret_cast<float*>(&filter);

    coefficients[0] = 2.0f;
    coefficients[1] = 3.0f;
    coefficients[2] = 4.0f;
    coefficients[3] = 5.0f;
    coefficients[4] = 6.0f;
    filterer.setFilter(filter);

    ipl::Array<float> dry{ 5 };
    dry.data()[0] = 1.0f;
    dry.data()[1] = 2.0f;
    dry.data()[2] = 3.0f;
    dry.data()[3] = 4.0f;
    dry.data()[4] = 5.0f;

    ipl::Array<float> wet{ 5 };

    filterer.apply(static_cast<int>(dry.size(0)), dry.data(), wet.data());

    REQUIRE(wet.data()[0] == Approx(4.0));
    REQUIRE(wet.data()[1] == Approx(5.0));
    REQUIRE(wet.data()[2] == Approx(6.0));
    REQUIRE(wet.data()[3] == Approx(16.0));
    REQUIRE(wet.data()[4] == Approx(8.0));
}