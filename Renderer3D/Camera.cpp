#include "Camera.h"
#include <DirectXMath.h>

using namespace Eigen;

constexpr uint64_t MATRIX_SIZE = sizeof(Eigen::Matrix4f);
using namespace DirectX;

Camera::Camera(
	const Eigen::Vector3f& pos,
	const Eigen::Vector3f& lookDir,
	const Eigen::Vector3f& up,
	char* mmapPtr)
	:
	mmapPtr(mmapPtr), lookDir(lookDir), up(up), pos(pos)
{
	view = Matrix4f::Zero();
	proj = Matrix4f::Zero();
}

void Camera::UpdateViewMatrix()
{
	lookDir.normalize();
	Vector3f xAxis = up.cross(lookDir).normalized();
	Vector3f yAxis = lookDir.cross(xAxis);
	Vector3f negPos = -pos;

	view(0, 0) = xAxis(0);
	view(0, 1) = xAxis(1);
	view(0, 2) = xAxis(2);
	view(0, 3) = xAxis.dot(negPos);

	view(1, 0) = yAxis(0);
	view(1, 1) = yAxis(1);
	view(1, 2) = yAxis(2);
	view(1, 3) = yAxis.dot(negPos);

	view(2, 0) = lookDir(0);
	view(2, 1) = lookDir(1);
	view(2, 2) = lookDir(2);
	view(2, 3) = lookDir.dot(negPos);

	view(3, 3) = 1;

	memcpy(mmapPtr, view.data(), MATRIX_SIZE);
}
void Camera::UpdateProjMatrix(
	float FovAngleY,
	float AspectRatio,
	float NearZ,
	float FarZ)
{
	float tanHalfFovy = tan(FovAngleY * 0.5f);
	float fRange = FarZ / (FarZ - NearZ);

	proj(0, 0) = static_cast<float>(1) / (AspectRatio * tanHalfFovy);
	proj(1, 1) = -static_cast<float>(1) / (tanHalfFovy);
	proj(2, 2) = fRange;
	proj(2, 3) = -fRange * NearZ;
	proj(3, 2) = 1.0f;

	memcpy(mmapPtr + MATRIX_SIZE, proj.data(), MATRIX_SIZE);
}

void Camera::ProcessUserInput(
	Window* window,
	float dt)
{
	if (window->IsKeyPressed('W')) { pos = pos + lookDir * dt; }
	if (window->IsKeyPressed('S')) { pos = pos - lookDir * dt; }
	if (window->IsKeyPressed(VK_SPACE)) { pos = pos + up * dt; }
	if (window->IsKeyPressed(VK_CONTROL)) { pos = pos - up * dt; }
	if (window->IsKeyPressed('D')) { pos = pos + up.cross(lookDir) * dt; }
	if (window->IsKeyPressed('A')) { pos = pos - up.cross(lookDir) * dt; }

	UpdateViewMatrix();
}
