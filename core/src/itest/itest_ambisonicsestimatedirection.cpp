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

#include <ambisonics_binaural_effect.h>
#include <ambisonics_rotate_effect.h>
#include <profiler.h>
#include <algorithm>
#include <numeric>
#include "helpers.h"
#include "ui_window.h"
using namespace ipl;

#include <phonon.h>

#include "itest.h"

void generateSamplesOnSphere(int count, float radius, Vector3f* points)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    for (int i = 0; i < count; ++i)
    {
        double u = dis(gen); // 0 to 1
        double v = dis(gen); // 0 to 1

        double theta = 2.0 * Math::kPiD * u;        // Azimuth
        double phi = std::acos(2.0 * v - 1.0); // Polar angle (colatitude)

        points[i].x() = static_cast<float>(radius * std::sin(phi) * std::cos(theta));
        points[i].y() = static_cast<float>(radius * std::sin(phi) * std::sin(theta));
        points[i].z() = static_cast<float>(radius * std::cos(phi));
    }
}

void buildVertexNeighbors(int numVertices, std::vector<Triangle>& triangles, std::vector<std::vector<int>>& neighbors)
{
    std::vector<std::unordered_set<int>> nbrSets(numVertices);

    for (size_t i = 0; i < triangles.size(); ++i)
    {
        Triangle& tri = triangles[i];
        int v0 = tri.indices[0];
        int v1 = tri.indices[1];
        int v2 = tri.indices[2];

        nbrSets[v0].insert(v1);
        nbrSets[v0].insert(v2);

        nbrSets[v1].insert(v0); 
        nbrSets[v1].insert(v2);

        nbrSets[v2].insert(v0);
        nbrSets[v2].insert(v1);
    }

    neighbors.clear();
    neighbors.resize(numVertices);
    for (size_t i = 0; i < numVertices; ++i)
    {
        neighbors[i].assign(nbrSets[i].begin(), nbrSets[i].end());
    }
}

void generateTriangulatedSphere(int level, float radius, std::vector<Vector3f> &vertices, std::vector<Triangle> &triangles)
{
    std::unordered_map<uint64_t, int> midpointCache;
    midpointCache.clear();

    vertices.clear();
    triangles.clear();

    // create 12 vertices of a regular icosahedron
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    auto addVertex = [&radius, &vertices](float x, float y, float z)
    {
        Vector3f v = Vector3f::unitVector(Vector3f(x, y, z)) * radius;
        vertices.push_back(v);
    };

    addVertex(-1, t, 0);
    addVertex(1, t, 0);
    addVertex(-1, -t, 0);
    addVertex(1, -t, 0);

    addVertex(0, -1, t);
    addVertex(0, 1, t);
    addVertex(0, -1, -t);
    addVertex(0, 1, -t);

    addVertex(t, 0, -1);
    addVertex(t, 0, 1);
    addVertex(-t, 0, -1);
    addVertex(-t, 0, 1);

    // create 20 triangles of the icosahedron
    triangles =
    {
        {0,11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7,10}, {0,10,11},
        {1, 5, 9}, {5,11, 4}, {11,10,2}, {10,7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4,11}, {6, 2,10}, {8, 6, 7}, {9, 8, 1}
    };

    auto packKey = [](int a, int b)
    {
        return (uint64_t(std::min(a, b)) << 32) | uint32_t(std::max(a, b));
    };

    auto getMiddlePoint = [packKey, &midpointCache](int p1, int p2, std::vector<Vector3f>& vertices, float radius)
    {
        uint64_t key = packKey(p1, p2);
        auto it = midpointCache.find(key);
        if (it != midpointCache.end())
        {
            return it->second;
        }

        Vector3f point1 = vertices[p1];
        Vector3f point2 = vertices[p2];
        Vector3f midPoint = Vector3f::unitVector((point1 + point2) * 0.5f) * radius;

        int newIndex = static_cast<int>(vertices.size());
        vertices.push_back(midPoint);
        midpointCache[key] = newIndex;
        return newIndex;
    };

    // Subdivide.
    for (int i = 0; i < level; ++i)
    {
        std::vector<Triangle> newFaces;
        newFaces.reserve(static_cast<int>(static_cast<int>(triangles.size())) * 4);

        for (auto& tri : triangles)
        {
            int a = getMiddlePoint(tri.indices[0], tri.indices[1], vertices, radius);
            int b = getMiddlePoint(tri.indices[1], tri.indices[2], vertices, radius);
            int c = getMiddlePoint(tri.indices[2], tri.indices[0], vertices, radius);

            newFaces.push_back({ tri.indices[0], a, c });
            newFaces.push_back({ tri.indices[1], b, a });
            newFaces.push_back({ tri.indices[2], c, b });
            newFaces.push_back({ a, b, c });
        }

        triangles.swap(newFaces);
    }
}

void generateSourceWeights(int numSources, Vector3f* sources, bool uniformSourceWeights, float* weights)
{
    float weightSum = .0f;
    Vector3f averageDirection = Vector3f::kZero;

    for (int i = 0; i < numSources; ++i)
    {
        weights[i] = uniformSourceWeights ? 1.0f / numSources : std::max<float>(0.1f, std::fabsf(static_cast<float>(rand()) / static_cast<float>(RAND_MAX)));
        weightSum += weights[i];
    }

    for (int i = 0; i < numSources; ++i)
    {
        weights[i] = weights[i] / weightSum;
        printf("S%02d: (%.2f %.2f %.2f) %.2f\n", i, sources[i].x(), sources[i].y(), sources[i].z(), weights[i]);
        averageDirection += (sources[i] * weights[i]);
    }

    averageDirection = Vector3f::unitVector(averageDirection);
    printf("AVG: (%.2f %.2f %.2f)\n", averageDirection.x(), averageDirection.y(), averageDirection.z());
}


void generateRandomSources(int numSources, Vector3f* sources)
{
    for (int i = 0; i < numSources; ++i)
    {
        generateSamplesOnSphere(1, 1.0f, &sources[i]);
        sources[i] = Vector3f::unitVector(sources[i]);
    }
}

void generateAmbisonicsField(int order, int numSources, Vector3f* sources, float* weights, float* field)
{
    for (int i = 0; i < numSources; ++i)
    {
        SphericalHarmonics::projectSinglePointAndUpdate(sources[i], order, weights[i], field);
    }

    if (numSources == 0)
    {
        // Generate a random field on the sphere.
        int numCoeffs = SphericalHarmonics::numCoeffsForOrder(order);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-1.0, 1.0);

        for (int i = 0; i < numCoeffs; ++i)
        {
            field[i] = static_cast<float>(dis(gen));
        }
    }
}

void evaluateAmbisonicsField(int order, int numSamples, Vector3f* samples, float* field, float* evaluatedField)
{
    for (int i = 0; i < numSamples; ++i)
    {
        evaluatedField[i] = SphericalHarmonics::evaluateSum(order, field, Vector3f::unitVector(samples[i]));
    }
}

void evaluatePerChannelAmbisonicsField(int order, int numSamples, Vector3f* samples, float* field, ipl::DynamicMatrixf& perChannelDirectionField)
{
    for (int i = 0; i < numSamples; ++i)
    {
        for (auto l = 0, index = 0; l <= order; ++l)
        {
            for (auto m = -l; m <= l; ++m, ++index)
            {
                perChannelDirectionField(index, i) = SphericalHarmonics::evaluate(l, m, Vector3f::unitVector(samples[i]));
            }
        }
    }
}

float linearTodB(float value)
{
    if (value > .0f)
        return 20.0f * log10(value);

    return -200.0f;
}

UIColor getHeatMapColor(float linearValue)
{
    float value = std::max<float>(0.0f, std::min<float>(1.0f, std::abs<float>(linearValue)));
    value = linearTodB(value);

    const int numColors = 6;
    UIColor colors[numColors] = { UIColor{.1f, .1f, .1f}, UIColor::kBlue, UIColor::kGreen, UIColor::kYellow, UIColor{1.0f, .65f, .0f},  UIColor::kRed};
    UIColor result = colors[0];

    float dbCutoffs[numColors] = { -20.0f, -16.0f, -12.0f, -8.0f, -4.0f, .0f };

    if (value <= dbCutoffs[0]) return colors[0];
    if (value >= dbCutoffs[numColors - 1]) return colors[numColors - 1];

    for (int i = 0; i < numColors - 1; ++i) {
        if (value >= dbCutoffs[i] && value <= dbCutoffs[i + 1])
        {
            float t = (value - dbCutoffs[i]) / (dbCutoffs[i + 1] - dbCutoffs[i]);
            result.r = (1.0f - t) * colors[i].r + t * colors[i + 1].r;
            result.g = (1.0f - t) * colors[i].g + t * colors[i + 1].g;
            result.b = (1.0f - t) * colors[i].b + t * colors[i + 1].b;
        }
    }

    return result;
}

void colorAmbisonicsField(int numSamples, float* evaluatedField, UIColor* colors)
{
    for (int i = 0; i < numSamples; ++i)
    {
        colors[i] = getHeatMapColor(evaluatedField[i]);
    }
}

void normalizeAmbisonicsField(int numSamples, float* evaluatedField)
{
    float maxValue = FLT_MIN;
    for (int i = 0; i < numSamples; ++i)
    {
        maxValue = std::max<float>(maxValue, std::abs<float>(evaluatedField[i]));
    }

    for (int i = 0; i < numSamples; ++i)
    {
        evaluatedField[i] = evaluatedField[i] / maxValue;
    }
}

void estimateDoAFromFOAAcousticIntensityVector(int order, float sphereRadius, float* field, std::vector<Vector3f>& estimatedDOAs)
{
    if (order <= 0)
        return;

    Timer timer;
    timer.start();

    Vector3f doa;
    doa.x() = -field[1] * field[0];
    doa.y() = field[2] * field[0];
    doa.z() = -field[3] * field[0];
    doa = Vector3f::unitVector(doa);

    double timeElapsed = timer.elapsedNanoseconds();
    double minTimeElapsed = 100.0;  // Min resolution.

    printf("----\nEstimated DoA (FOA): %d direction, %.2lf ns (%.1f FPmS)\n", 1, timeElapsed, 1e6 / std::max<double>(minTimeElapsed, timeElapsed));
    printf("D%02d: (%.2f %.2f %.2f)\n", 0, doa.x(), doa.y(), doa.z());

    doa *= sphereRadius;
    estimatedDOAs.clear();
    estimatedDOAs.push_back(doa);
}

void runSteeredResponsePower(float sphereRadius, int numDirections, Vector3f* directions, std::vector<std::vector<int>>& neighbors, float* doAField, float estimationThreshold, std::vector<Vector3f>& estimatedDOAs)
{
    // Brute Force peak find algorithm.
    std::vector<std::pair<float, int>> sortedDirection;
    estimatedDOAs.clear();

    for (int i = 0; i < numDirections; ++i)
    {
        std::vector<int>& vertexNeighbors = neighbors[i];
        float vertexIntensity = doAField[i];
        bool isLocalMaxima = true && (vertexNeighbors.size() > 0);
        for (int j = 0; (j < static_cast<int>(vertexNeighbors.size())) && isLocalMaxima; ++j)
        {
            if (doAField[vertexNeighbors[j]] > doAField[i])
                isLocalMaxima = false;
        }

        if (isLocalMaxima)
        {
            sortedDirection.push_back(std::pair<float, int>(doAField[i], i));
        }
    }

    if (sortedDirection.size() == 0)
        return;

    std::sort(sortedDirection.begin(), sortedDirection.end(), [](std::pair<float, int> a, std::pair<float, int> b) { return a.first > b.first; });
    float maxValue = sortedDirection[0].first;

    for (int i = 0; i < sortedDirection.size(); ++i)
    {
        Vector3f& doa = Vector3f::unitVector(directions[sortedDirection[i].second]);
        if (sortedDirection[i].first > estimationThreshold * maxValue)
        {
            estimatedDOAs.push_back(sphereRadius * doa);
        }
    }

}

void estimateDoAFromSteeredResponsePower(int order, float sphereRadius, float* field, std::vector<Vector3f>& directions, std::vector<std::vector<int>>& neighbors, float* doAField, float estimationThreshold, std::vector<Vector3f>& estimatedDOAs)
{
    Timer timer;
    timer.start();

    evaluateAmbisonicsField(order, static_cast<int>(directions.size()), directions.data(), field, doAField);
    runSteeredResponsePower(sphereRadius, static_cast<int>(directions.size()), directions.data(), neighbors, doAField, estimationThreshold, estimatedDOAs);
    
    double timeElapsed = timer.elapsedMicroseconds();

    printf("----\nEstimated DoA (SRP): %d directions, %.2lf us (%.1f FPmS) (%d directions)\n", static_cast<int>(estimatedDOAs.size()), timeElapsed, 1e3 / timeElapsed, static_cast<int>(directions.size()));
    for (int i = 0; i < estimatedDOAs.size(); ++i)
    {
        Vector3f& doa = Vector3f::unitVector(estimatedDOAs[i]);
        printf("D%02d: (%.2f %.2f %.2f)\n", i, doa.x(), doa.y(), doa.z());
    }
}

void runCompressiveSensing(int order, float sphereRadius, int numDirections, Vector3f* directions, float* field, ipl::DynamicMatrixf& directionsField, std::vector<Vector3f>& estimatedDOAs)
{
    estimatedDOAs.clear();
    std::vector<float> x(numDirections);
    ipl::DynamicMatrixf b(SphericalHarmonics::numCoeffsForOrder(order), 1, field);

    leastSquaresL1(directionsField, b, x.data());

    std::vector<int> idx(numDirections);
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(), [&x](int i1, int i2) {return x[i1] > x[i2]; });

    std::vector<Vector3f> tempEstimatedDOAs;
    std::vector<float> tempEstimatedDOAWeights;
    float threshold = .05f;
    for (int i = 0; i < numDirections; ++i)
    {
        if (x[idx[i]] > threshold)
        {
            Vector3f& doa = Vector3f::unitVector(directions[idx[i]]);
            tempEstimatedDOAs.push_back(doa);
            tempEstimatedDOAWeights.push_back(x[idx[i]]);
        }
    }

    // Cluster temporary directions and output
    float clusterThreshold = std::max<float>(70.0f / numDirections, .3f);
    std::vector<float> estimatedWeight;
    for (auto i = 0; i < tempEstimatedDOAs.size(); ++i)
    {
        bool clusterFound = false;
        for (auto j = 0; j < estimatedDOAs.size(); ++j)
        {
            if ((tempEstimatedDOAs[i] - estimatedDOAs[j]).length() < clusterThreshold)
            {
                clusterFound = true;
                estimatedDOAs[j] = Vector3f::unitVector((estimatedDOAs[j] * estimatedWeight[j]) + (tempEstimatedDOAWeights[i] * tempEstimatedDOAs[i]));
                estimatedWeight[j] += tempEstimatedDOAWeights[j];
            }
        }

        if (!clusterFound)
        {
            estimatedDOAs.push_back(tempEstimatedDOAs[i]);
            estimatedWeight.push_back(tempEstimatedDOAWeights[i]);
        }
    }

    for (auto j = 0; j < estimatedDOAs.size(); ++j)
    {
        estimatedDOAs[j] = sphereRadius * estimatedDOAs[j];
    }
}

void estimateDoAFromCompressiveSensing(int order, float sourceRadius, float* field, std::vector<Vector3f>& directions, ipl::DynamicMatrixf& perChannelDirectionField, std::vector<Vector3f>& estimatedDOAs)
{
    Timer timer;
    timer.start();

    evaluatePerChannelAmbisonicsField(order, static_cast<int>(directions.size()), directions.data(), field, perChannelDirectionField);
    runCompressiveSensing(order, sourceRadius, static_cast<int>(directions.size()), directions.data(), field, perChannelDirectionField, estimatedDOAs);

    double timeElapsed = timer.elapsedMicroseconds();

    printf("----\nEstimated DoA (CS): %d directions, %.2lf us (%.1f FPmS) (%d directions)\n", static_cast<int>(estimatedDOAs.size()), timeElapsed, 1e3 / timeElapsed, static_cast<int>(directions.size()));
    for (int i = 0; i < estimatedDOAs.size(); ++i)
    {
        Vector3f& doa = Vector3f::unitVector(estimatedDOAs[i]);
        printf("D%02d: (%.2f %.2f %.2f)\n", i, doa.x(), doa.y(), doa.z());
    }
}

void estimateDoAFromLeastSquareFitting(int order, float sphereRadius, float* field, std::vector<Vector3f>& estimatedDOAs)
{
    estimatedDOAs.clear();
    printf("Estimated DoA (Least Square Fitting): WIP\n");
}

ITEST(ambisonicsestimatedirection)
{
    bool uniformSourceWeights = true;
    int numSources = 2;
    std::vector<Vector3f> sources;
    std::vector<float> weights;

    const int maxOrder = 8;
    float* field = new float[SphericalHarmonics::numCoeffsForOrder(maxOrder)];
    memset(field, 0, sizeof(float) * SphericalHarmonics::numCoeffsForOrder(maxOrder));

    int order = 1;
    float sphereRadius = 10.0f;
    float sourceRadius = 20.0f;
    float estimationThreshold = 0.5f;

    int level = 5;
    std::vector<Triangle> triangles;
    std::vector<Vector3f> vertices;
    generateTriangulatedSphere(level, sphereRadius, vertices, triangles);

    std::vector<float> evaluatedField;
    std::vector<UIColor> colors;
    std::vector<Vector3f> estimatedDOAs;
    std::vector<float> doAField;
    ipl::DynamicMatrixf doAPerChannelField;
    
    bool enableSmoothShading = false;
    int currentDoAOption = 0;
    enum DoAOptions {FOA, SRP, CS, LSF};
    const char* doAOptions[] = {"FOA Acoustic Intensity Vector", "Steered-Response Power", "Compressive Sensing", "Least-Squares Fitting"};
    
    int srpLevel = 2;
    std::vector<Vector3f> srpDirections;
    std::vector<std::vector<int>> srpDirectionNeighbors;

    bool resampleSources = true;
    bool reweightSources = false;
    bool generateSourceField = true;
    bool generateSphereGrid = true;
    bool evaluateSphereGridField = true;
    bool estimateDOA = true;

    auto gui = [&]()
    {
        if (ImGui::SliderInt("#Sources", &numSources, 1, 5))
        {
            resampleSources = true;
            generateSourceField = true;
            evaluateSphereGridField = true;
            estimateDOA = true;
        }

        if (ImGui::Checkbox("Uniform Source Weights", &uniformSourceWeights))
        {
            reweightSources = true;
            generateSourceField = true;
            evaluateSphereGridField = true;
            estimateDOA = true;
        }

        if (ImGui::Button("Rasample Sources"))
        {
            resampleSources = true;
            generateSourceField = true;
            evaluateSphereGridField = true;
            estimateDOA = true;
        }

        if (ImGui::SliderInt("Ambisonics Order", &order, 0, maxOrder))
        {
            generateSphereGrid = true;
            evaluateSphereGridField = true;
            estimateDOA = true;
        }

        ImGui::Checkbox("Smooth Shading", &enableSmoothShading);

        if (ImGui::Combo("DoA Estimation##combo", &currentDoAOption, doAOptions, IM_ARRAYSIZE(doAOptions)))
        {
            estimateDOA = true;
        }

        if (currentDoAOption == DoAOptions::SRP)
        {
            if (ImGui::SliderFloat("Peak Fraction Threshold", &estimationThreshold, .1f, 1.0f))
            {
                estimateDOA = true;
            }
        }

        if (currentDoAOption == DoAOptions::SRP || currentDoAOption == DoAOptions::CS)
        {
            if (ImGui::SliderInt("DOA Sampling Level", &srpLevel, 0, 5) || srpDirections.size() == 0)
            {
                generateSphereGrid = true;
                estimateDOA = true;
            }
        }

        if (resampleSources)
        {
            printf("\nGenerating %d sources...\n", numSources);
            sources.resize(numSources);
            weights.resize(numSources);
            generateRandomSources(numSources, sources.data());
            generateSourceWeights(numSources, sources.data(), uniformSourceWeights, weights.data());
            resampleSources = false;
        }

        if (reweightSources)
        {
            printf("\nRe-weighting %d sources...\n", numSources);
            weights.resize(numSources);
            generateSourceWeights(numSources, sources.data(), uniformSourceWeights, weights.data());
            reweightSources = false;
        }

        if (generateSourceField)
        {
            memset(field, 0, sizeof(float)* SphericalHarmonics::numCoeffsForOrder(maxOrder));
            generateAmbisonicsField(maxOrder, numSources, sources.data(), weights.data(), field);
            generateSourceField = false;
        }

        if (generateSphereGrid)
        {
            std::vector<Triangle> triangles;
            generateTriangulatedSphere(srpLevel, 1.0f, srpDirections, triangles);
            buildVertexNeighbors(static_cast<int>(srpDirections.size()), triangles, srpDirectionNeighbors);

            doAField.resize(srpDirections.size());
            doAPerChannelField.resize(SphericalHarmonics::numCoeffsForOrder(order), static_cast<int>(srpDirections.size()));
            generateSphereGrid = false;
        }

        if (evaluateSphereGridField)
        {
            evaluatedField.resize(vertices.size());
            colors.resize(vertices.size());

            evaluateAmbisonicsField(order, static_cast<int>(vertices.size()), vertices.data(), field, evaluatedField.data());
            normalizeAmbisonicsField(static_cast<int>(evaluatedField.size()), evaluatedField.data());
            colorAmbisonicsField(static_cast<int>(evaluatedField.size()), evaluatedField.data(), colors.data());
            evaluateSphereGridField = false;
        }

        if (estimateDOA)
        {
            if (currentDoAOption == DoAOptions::FOA)
            {
                estimateDoAFromFOAAcousticIntensityVector(order, sourceRadius, field, estimatedDOAs);
            }
            else if (currentDoAOption == DoAOptions::SRP)
            {
                // Compute Ambisonic field in various directions and find local maximas to identify sources.
                estimateDoAFromSteeredResponsePower(order, sourceRadius, field, srpDirections, srpDirectionNeighbors, doAField.data(), estimationThreshold, estimatedDOAs);
            }
            else if (currentDoAOption == DoAOptions::CS)
            {
                // Compute Ambisonic field in various directions, a = Yx. Now, solve min_x ||Yx - a||_2^2 + L||x||_1
                // doAPerChannelField is #Channels x #Directionx.
                estimateDoAFromCompressiveSensing(order, sourceRadius, field, srpDirections, doAPerChannelField, estimatedDOAs);
            }
            else if (currentDoAOption == DoAOptions::LSF)
            {
                // Directly solve the non-linear square fitting, 
                estimateDoAFromLeastSquareFitting(order, sourceRadius, field, estimatedDOAs);
            }

            estimateDOA = false;
        }
    };

    auto display = [&]()
    {
        for (int i = 0; i < static_cast<int>(sources.size()); ++i)
        {
            UIColor color = { .0f, .0f, 1.0f };
            Vector3f source = { sources[i].x() * sourceRadius, sources[i].y() * sourceRadius, sources[i].z() * sourceRadius };
            UIWindow::drawPoint(source, UIColor::kBlue, 20.0f);
            UIWindow::drawLineSegment(Vector3f::kZero, source, color, 2.0f);
        }

        if (enableSmoothShading)
        {
            for (int i = 0; i < static_cast<int>(triangles.size()); ++i)
            {
                Triangle& tri = triangles[i];
                Vector3f& v0 = vertices[tri.indices[0]];
                Vector3f& v1 = vertices[tri.indices[1]];
                Vector3f& v2 = vertices[tri.indices[2]];

                UIColor& c0 = colors[tri.indices[0]];
                UIColor& c1 = colors[tri.indices[1]];
                UIColor& c2 = colors[tri.indices[2]];

                UIWindow::drawTriangle(v0, v1, v2, c0, c1, c2);
            }
        }
        else
        {
            for (int i = 0; i < static_cast<int>(vertices.size()); ++i)
            {
                UIWindow::drawPoint(vertices[i], colors[i], 4.0f);
            }

            for (int i = 0; i < static_cast<int>(triangles.size()); ++i)
            {
                Triangle& tri = triangles[i];
                Vector3f& v0 = vertices[tri.indices[0]];
                Vector3f& v1 = vertices[tri.indices[1]];
                Vector3f& v2 = vertices[tri.indices[2]];

                UIColor& c0 = colors[tri.indices[0]];
                UIColor& c1 = colors[tri.indices[1]];
                UIColor& c2 = colors[tri.indices[2]];

                UIWindow::drawLineSegment(v0, v1, c0);
                UIWindow::drawLineSegment(v1, v2, c1);
                UIWindow::drawLineSegment(v2, v0, c2);
            }
        }

        for (int i = 0; i < estimatedDOAs.size(); ++i)
        {
            UIWindow::drawPoint(estimatedDOAs[i], UIColor::kMagenta, 25.0f);
            UIWindow::drawLineSegment(Vector3f::kZero, estimatedDOAs[i], UIColor::kMagenta, 2.0f);
        }
    };

    UIWindow::sCamera = CoordinateSpace3f{ Vector3f{ -1.0f, .0f, .0f }, UIWindow::sCamera.up, Vector3f{ 20.0, .0f, .0f } };
    UIWindow::sMovementSpeed = 10.0f;

    UIWindow window;
    window.run(gui, display, nullptr);

    delete[] field;
}
