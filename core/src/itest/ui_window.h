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

#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#include <Windows.h>

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#include <functional>

#include <coordinate_space.h>
#include <ray.h>
#include <scene.h>

#include "ui_audio_engine.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// UIColor
// --------------------------------------------------------------------------------------------------------------------

union UIColor
{
    static const UIColor kWhite;
    static const UIColor kBlack;
    static const UIColor kRed;
    static const UIColor kGreen;
    static const UIColor kBlue;
    static const UIColor kYellow;
    static const UIColor kMagenta;
    static const UIColor kCyan;

    struct
    {
        float r, g, b;
    };

    float elements[3];
};


// --------------------------------------------------------------------------------------------------------------------
// UIWindow
// --------------------------------------------------------------------------------------------------------------------

typedef std::function<void(void)> UIGUICallback;
typedef std::function<void(void)> UIDisplayCallback;

class UIWindow
{
public:
    static CoordinateSpace3f sCamera;
    static float sMovementSpeed;

    UIWindow();

    ~UIWindow();

    void run(UIGUICallback gui,
             UIDisplayCallback display,
			 AudioCallback audio,
             AudioTailCallback audioTail = nullptr);

    static void drawPoint(const Vector3f& point,
                          const UIColor& color,
                          float size = 20.0f);

    static void drawLineSegment(const Vector3f& p, const Vector3f& q, const UIColor& color, float width = 1.0f);
    static void drawRay(const Ray& ray, const UIColor& color, float width = 1.0f);
    static void drawBox(const Box& box, const UIColor& color);
    static void drawMesh(const Mesh& mesh);
    static void drawImage(const float* image, int width, int height);
    static Vector3f screenToWorld(std::shared_ptr<IScene> scene, float offset);
    static bool dragMode();

private:
    static GLFWwindow* sWindow;
    static ImGuiIO* sIO;
    static int sWidth;
    static int sHeight;
    static bool sMoveForward;
    static bool sMoveBackward;
    static bool sMoveLeft;
    static bool sMoveRight;
    static bool sMoveUp;
    static bool sMoveDown;
    static bool sMouseDown;
    static bool sLeftMouseDown;
    static int sMouseX;
    static int sMouseY;
    static int sMouseDx;
    static int sMouseDy;
    static double sTime;
	static unique_ptr<UIAudioEngine> sAudioEngine;

    static void keyboard(GLFWwindow* window,
                         int key,
                         int scanCode,
                         int action,
                         int mods);

    static void mouseClick(GLFWwindow* window,
                           int button,
                           int action,
                           int mods);

    static void mouseMove(GLFWwindow* window,
                          double x,
                          double y);

    static void updateCamera();

    static void rotateCamera(float dTheta,
                             float dPhi);

    static Vector3f rotateBy(const Vector3f& v,
                             float angle,
                             const Vector3f& axis);
};

}
