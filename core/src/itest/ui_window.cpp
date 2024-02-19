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

#include "ui_window.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// UIColor
// --------------------------------------------------------------------------------------------------------------------

const UIColor UIColor::kWhite = {1.0f, 1.0f, 1.0f};
const UIColor UIColor::kBlack = {0.0f, 0.0f, 0.0f};
const UIColor UIColor::kRed = {1.0f, 0.0f, 0.0f};
const UIColor UIColor::kGreen = {0.0f, 1.0f, 0.0f};
const UIColor UIColor::kBlue = {0.0f, 0.0f, 1.0f};
const UIColor UIColor::kYellow = {1.0f, 1.0f, 0.0f};
const UIColor UIColor::kMagenta = {1.0f, 0.0f, 1.0f};
const UIColor UIColor::kCyan = {0.0f, 1.0f, 1.0f};


// --------------------------------------------------------------------------------------------------------------------
// UIWindow
// --------------------------------------------------------------------------------------------------------------------

GLFWwindow* UIWindow::sWindow = nullptr;
ImGuiIO* UIWindow::sIO = nullptr;
int UIWindow::sWidth = 0;
int UIWindow::sHeight = 0;
CoordinateSpace3f UIWindow::sCamera{};
float UIWindow::sMovementSpeed = 2.0f;
bool UIWindow::sMoveForward = false;
bool UIWindow::sMoveBackward = false;
bool UIWindow::sMoveLeft = false;
bool UIWindow::sMoveRight = false;
bool UIWindow::sMoveUp = false;
bool UIWindow::sMoveDown = false;
bool UIWindow::sMouseDown = false;
bool UIWindow::sLeftMouseDown = false;
int UIWindow::sMouseX = 0;
int UIWindow::sMouseY = 0;
int UIWindow::sMouseDx = 0;
int UIWindow::sMouseDy = 0;
double UIWindow::sTime = 0.0;
unique_ptr<UIAudioEngine> UIWindow::sAudioEngine = nullptr;

UIWindow::UIWindow()
{
    glfwInit();
    sWindow = glfwCreateWindow(1280, 720, "Steam Audio", nullptr, nullptr);
    glfwMakeContextCurrent(sWindow);
    glfwSwapInterval(1);

    sWidth = 1280;
    sHeight = 720;

    glfwSetKeyCallback(sWindow, keyboard);
    glfwSetMouseButtonCallback(sWindow, mouseClick);
    glfwSetCursorPosCallback(sWindow, mouseMove);

    ImGui::CreateContext();
    sIO = &ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(sWindow, true);
    ImGui_ImplOpenGL2_Init();
}

UIWindow::~UIWindow()
{
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwDestroyWindow(sWindow);
    glfwTerminate();
}

void UIWindow::run(UIGUICallback gui,
				   UIDisplayCallback display,
				   AudioCallback audio,
                   AudioTailCallback audioTail /* = nullptr */)
{
	if (audio)
	{
		sAudioEngine = ipl::make_unique<UIAudioEngine>(44100, 1024, audio, audioTail);
	}

    while (!glfwWindowShouldClose(sWindow))
    {
        glfwPollEvents();
        updateCamera();

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

		if (audio)
		{
			static auto audioClipIndex = -1;
			if (ImGui::Combo("Audio Clip", &audioClipIndex, sAudioEngine->mAudioClipsStr.data(), static_cast<int>(sAudioEngine->mAudioClipsStr.size())))
			{
				sAudioEngine->play(audioClipIndex);
			}

            if (ImGui::Button("Stop"))
            {
                sAudioEngine->stop();
            }

			ImGui::Spacing();
		}

		if (gui)
			gui();

        ImGui::Render();

        glfwGetFramebufferSize(sWindow, &sWidth, &sHeight);
        glViewport(0, 0, sWidth, sHeight);

        glCullFace(GL_NONE);
        glDepthFunc(GL_LESS);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_POINT_SMOOTH);
        glShadeModel(GL_FLAT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(90.0f, static_cast<float>(sWidth) / static_cast<float>(sHeight), 1e-2f, 1e6f);

        const auto& eye = sCamera.origin;
        const auto& ahead = sCamera.ahead;
        const auto& up = sCamera.up;

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(eye.x(), eye.y(), eye.z(), eye.x() + ahead.x(), eye.y() + ahead.y(), eye.z() + ahead.z(), up.x(), up.y(), up.z());

        if (display)
            display();

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwMakeContextCurrent(sWindow);
        glfwSwapBuffers(sWindow);
    }
}

void UIWindow::keyboard(GLFWwindow* window, int key, int scanCode, int action, int mods)
{
    if (sIO && sIO->WantCaptureKeyboard)
        return;

    auto pressed = (action == GLFW_PRESS || action == GLFW_REPEAT);

    sMoveForward = (key == GLFW_KEY_UP && pressed) || (key == GLFW_KEY_W && pressed);
    sMoveBackward = (key == GLFW_KEY_DOWN && pressed) || (key == GLFW_KEY_S && pressed);
    sMoveLeft = (key == GLFW_KEY_LEFT && pressed) || (key == GLFW_KEY_A && pressed);
    sMoveRight = (key == GLFW_KEY_RIGHT && pressed) || (key == GLFW_KEY_D && pressed);
    sMoveUp = (key == GLFW_KEY_PAGE_UP && pressed);
    sMoveDown = (key == GLFW_KEY_PAGE_DOWN && pressed);
}

void UIWindow::mouseClick(GLFWwindow* window, int button, int action, int mods)
{
    if (sIO && sIO->WantCaptureMouse)
        return;

    sMouseDown = (action == GLFW_PRESS);
    sLeftMouseDown = (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT);
    sMouseDx = sMouseDy = 0;
}

void UIWindow::mouseMove(GLFWwindow* window, double x, double y)
{
    if (sIO && sIO->WantCaptureMouse)
        return;

    if (sMouseDown)
    {
        sMouseDx = static_cast<int>(x) - sMouseX;
        sMouseDy = static_cast<int>(y) - sMouseY;
    }

    sMouseX = static_cast<int>(x);
    sMouseY = static_cast<int>(y);
}

void UIWindow::updateCamera()
{
    auto time = glfwGetTime();
    auto timeElapsed = time - sTime;
    sTime = time;

    auto distanceMoved = static_cast<float>(sMovementSpeed * timeElapsed);

    auto eye = sCamera.origin;
    auto ahead = sCamera.ahead;
    auto up = sCamera.up;
    auto right = sCamera.right;

    if (sMoveForward)
        eye += ahead * distanceMoved;
    if (sMoveBackward)
        eye -= ahead * distanceMoved;
    if (sMoveLeft)
        eye -= right * distanceMoved;
    if (sMoveRight)
        eye += right * distanceMoved;
    if (sMoveUp)
        eye += up * distanceMoved;
    if (sMoveDown)
        eye -= up * distanceMoved;

    sCamera = CoordinateSpace3f(sCamera.ahead, sCamera.up, eye);

    auto dTheta = sMouseDy / 1000.0f;
    auto dPhi = sMouseDx / 1000.0f;
    if (!glfwGetMouseButton(sWindow, GLFW_MOUSE_BUTTON_LEFT))
        rotateCamera(-dTheta, -dPhi);
    sMouseDx = sMouseDy = 0;
}

void UIWindow::rotateCamera(float dTheta, float dPhi)
{
    auto reference = Vector3f::kYAxis;

    auto ahead = sCamera.ahead;
    auto up = sCamera.up;
    auto right = sCamera.right;

    ahead = rotateBy(ahead, dTheta, right);
    up = rotateBy(up, dTheta, right);

    ahead = rotateBy(ahead, dPhi, reference);
    up = rotateBy(up, dPhi, reference);
    right = rotateBy(right, dPhi, reference);

    ahead = Vector3f::unitVector(ahead);
    right = Vector3f::unitVector(right);
    up = Vector3f::unitVector(up);

    sCamera = CoordinateSpace3f(ahead, up, sCamera.origin);
}

Vector3f UIWindow::rotateBy(const Vector3f& v, float angle, const Vector3f& axis)
{
    return (cosf(angle) * v +
            sinf(angle) * Vector3f::cross(axis, v) +
            (1.0f - cosf(angle)) * Vector3f::dot(axis, v) * axis);
}

void UIWindow::drawPoint(const Vector3f& point, const UIColor& color, float size)
{
    glPointSize(size);
    glBegin(GL_POINTS);
    glColor3fv(color.elements);
    glVertex3fv(point.elements);
    glEnd();
}

void UIWindow::drawLineSegment(const Vector3f& p, const Vector3f& q, const UIColor& color, float width /* = 1.0f */)
{
    glLineWidth(width);
    glBegin(GL_LINES);
    glColor3fv(color.elements);
    glVertex3fv(p.elements);
    glVertex3fv(q.elements);
    glEnd();
}

void UIWindow::drawRay(const Ray& ray, const UIColor& color, float width /* = 1.0f */)
{
    drawLineSegment(ray.origin, ray.origin + ray.direction, color, width);
}

void UIWindow::drawBox(const Box& box, const UIColor& color)
{
    auto xMin = box.minCoordinates.x();
    auto yMin = box.minCoordinates.y();
    auto zMin = box.minCoordinates.z();
    auto xMax = box.maxCoordinates.x();
    auto yMax = box.maxCoordinates.y();
    auto zMax = box.maxCoordinates.z();

    glBegin(GL_LINES);
    glColor3fv(color.elements);
    glVertex3f(xMin, yMin, zMin); glVertex3f(xMax, yMin, zMin);
    glVertex3f(xMin, yMin, zMin); glVertex3f(xMin, yMax, zMin);
    glVertex3f(xMin, yMin, zMin); glVertex3f(xMin, yMin, zMax);
    glVertex3f(xMax, yMin, zMin); glVertex3f(xMax, yMax, zMin);
    glVertex3f(xMax, yMin, zMin); glVertex3f(xMax, yMin, zMax);
    glVertex3f(xMin, yMax, zMin); glVertex3f(xMax, yMax, zMin);
    glVertex3f(xMin, yMax, zMin); glVertex3f(xMin, yMax, zMax);
    glVertex3f(xMin, yMin, zMax); glVertex3f(xMax, yMin, zMax);
    glVertex3f(xMin, yMin, zMax); glVertex3f(xMin, yMax, zMax);
    glVertex3f(xMax, yMax, zMax); glVertex3f(xMin, yMax, zMax);
    glVertex3f(xMax, yMax, zMax); glVertex3f(xMax, yMin, zMax);
    glVertex3f(xMax, yMax, zMax); glVertex3f(xMax, yMax, zMin);
    glEnd();
}

void UIWindow::drawMesh(const Mesh& mesh)
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat material[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, material);

    auto lightDirection = Vector3f::unitVector(Vector3f{1.0f, -1.0f, -1.0f});
    GLfloat ambient[] = {0.1f, 0.1f, 0.1f, 1.0f};
    GLfloat diffuse[] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat position[] = {lightDirection.x(), lightDirection.y(), lightDirection.z(), 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);

    glBegin(GL_TRIANGLES);
    for (auto i = 0; i < mesh.numTriangles(); ++i)
    {
        const auto& triangle = mesh.triangle(i);
        glNormal3fv(mesh.normal(i).elements);

        for (auto j = 0; j < 3; ++j)
            glVertex3fv(mesh.triangleVertex(i, j).elements);
    }
    glEnd();

    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHTING);
}

void UIWindow::drawImage(const float* image, int width, int height)
{
    glPixelZoom(sWidth / static_cast<float>(width), sHeight / static_cast<float>(height));
    glDrawPixels(width, height, GL_RGBA, GL_FLOAT, image);
}

Vector3f UIWindow::screenToWorld(std::shared_ptr<IScene> scene, float offset)
{
    ImVec2 mousePos = ImGui::GetMousePos();
    float x = (2.0f * mousePos.x) / sWidth - 1.0f;
    float y = 1.0f - (2.0f * mousePos.y) / sHeight;

    ipl::Matrix4x4f projMatrix, invProjMatrix;
    glGetFloatv(GL_PROJECTION_MATRIX, projMatrix.elements);
    ipl::inverse(projMatrix, invProjMatrix);

    ipl::Vector4f rayClip(x, y, -1.0f, 1.0f);
    ipl::Vector4f rayEye = invProjMatrix * rayClip;
    rayEye.z() = -1.0f;
    rayEye.w() = 0.0f;

    ipl::Matrix4x4f viewMatrix, invViewMatrix;
    glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix.elements);
    ipl::inverse(viewMatrix, invViewMatrix);

    ipl::Vector4f rayWorld = invViewMatrix * rayEye;
    ipl::Vector3f rayWorldDir(rayWorld.x(), rayWorld.y(), rayWorld.z());

    ipl::Ray mouseRay{ sCamera.origin, rayWorldDir };
    Hit hit = scene->closestHit(mouseRay, .0f, FLT_MAX);

    return hit.isValid() ? mouseRay.pointAtDistance(hit.distance) + (hit.normal * offset) : Vector3f(.0f, .0f, .0f);
}

bool UIWindow::dragMode() { return sLeftMouseDown; }

}
