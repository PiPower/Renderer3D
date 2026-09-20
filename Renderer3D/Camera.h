#pragma once
#include <Eigen/Dense>

class Camera
{
public:
	Camera(
		const Eigen::Vector3f& pos,
		const Eigen::Vector3f& lookDir,
		const Eigen::Vector3f& up,
		char* mmapPtr);

	void UpdateViewMatrix();

	void UpdateProjMatrix(
		float FovAngleY,
		float AspectRatio,
		float NearZ,
		float FarZ);
private:
	char* mmapPtr;
	Eigen::Matrix4f proj;
	Eigen::Matrix4f view;
	Eigen::Vector3f lookDir;
	Eigen::Vector3f up;
	Eigen::Vector3f pos;
};

