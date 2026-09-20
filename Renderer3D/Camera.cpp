#include "Camera.h"

using namespace Eigen;

constexpr uint64_t MATRIX_SIZE = sizeof(Eigen::Matrix4f);

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
	view(1, 0) = xAxis(1);
	view(2, 0) = xAxis(2);
	view(3, 0) = xAxis.dot(negPos);

	view(0, 1) = yAxis(0);
	view(1, 1) = yAxis(1);
	view(2, 1) = yAxis(2);
	view(3, 1) = yAxis.dot(negPos);

	view(0, 2) = lookDir(0);
	view(1, 2) = lookDir(1);
	view(2, 2) = lookDir(2);
	view(3, 2) = lookDir.dot(negPos);

	memcpy(mmapPtr, view.data(), MATRIX_SIZE);
}

void Camera::UpdateProjMatrix(
	float ViewWidth, 
	float ViewHeight, 
	float NearZ,
	float FarZ)
{
	float TwoNearZ = NearZ + NearZ;
	float fRange = FarZ / (FarZ - NearZ);

	proj(0, 0) = TwoNearZ / ViewWidth;
	proj(1, 1) = TwoNearZ / ViewHeight;
	proj(2, 2) = -fRange;
	proj(3, 2) = -fRange * NearZ;

	memcpy(mmapPtr + MATRIX_SIZE, proj.data(), MATRIX_SIZE);
}
