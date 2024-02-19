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
