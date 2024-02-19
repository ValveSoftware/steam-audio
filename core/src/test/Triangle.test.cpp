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

#include <mesh.h>
#include <triangle.h>

TEST_CASE("Triangle normals are calculated correctly.", "[triangle]")
{
    ipl::Vector3f vertices[3] = {
        ipl::Vector3f(0.0f, 0.0f, 0.0f),
        ipl::Vector3f(1.0f, 0.0f, 0.0f),
        ipl::Vector3f(0.0f, 1.0f, 0.0f)
    };

    ipl::Triangle triangle;
    triangle.indices[0] = 0;
    triangle.indices[1] = 1;
    triangle.indices[2] = 2;

    ipl::Mesh mesh(3, 1, vertices, &triangle);

    auto normal = mesh.normal(0);

    REQUIRE(normal.x() == Approx(0.0));
    REQUIRE(normal.y() == Approx(0.0));
    REQUIRE(normal.z() == Approx(1.0));
}
