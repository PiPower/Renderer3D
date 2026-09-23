#pragma once
#include <Eigen/Dense>
#include "window.hpp"

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

	void ProcessUserInput(
		Window* window,
		float dt);
private:
	char* mmapPtr;
	Eigen::Matrix4f proj;
	Eigen::Matrix4f view;
	Eigen::Vector3f lookDir;
	Eigen::Vector3f up;
	Eigen::Vector3f pos;

	Eigen::Vector3f initLookDir;
	Eigen::Vector3f initUp;
};

