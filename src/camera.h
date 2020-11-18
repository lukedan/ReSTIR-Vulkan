#pragma once

#include <nvmath.h>

struct Camera {
public:
	nvmath::vec3f position{ 3.0f, 4.0f, 5.0f };
	nvmath::vec3f lookAt{ 0.0f, 0.0f, 0.0f };
	nvmath::vec3f worldUp{ 0.0f, 1.0f, 0.0f };
	float zNear = 0.01f;
	float zFar = 1000.0f;
	float fovYRadians = 0.5f * nv_pi; // 90 degrees
	float aspectRatio = 1.0f;

	// attributes below need to be computed using recomputeAttributes()
	nvmath::vec3f unitForward;
	nvmath::vec3f unitRight;
	nvmath::vec3f unitUp;

	nvmath::mat4f viewMatrix;
	nvmath::mat4f projectionMatrix;
	nvmath::mat4f projectionViewMatrix;
	nvmath::mat4f inverseViewMatrix;

	void recomputeAttributes() {
		unitForward = nvmath::normalize(lookAt - position);
		unitRight = nvmath::normalize(nvmath::cross(unitForward, worldUp));
		unitUp = nvmath::cross(unitRight, unitForward);
		
		nvmath::mat3f rotation;
		rotation.set_row(0, unitRight);
		rotation.set_row(1, -unitUp);
		rotation.set_row(2, unitForward);
		nvmath::vec3f offset = -(rotation * position);

		viewMatrix.set_row(0, nvmath::vec4f(rotation.row(0), offset.x));
		viewMatrix.set_row(1, nvmath::vec4f(rotation.row(1), offset.y));
		viewMatrix.set_row(2, nvmath::vec4f(rotation.row(2), offset.z));
		viewMatrix.set_row(3, nvmath::vec4f_w);

		float f = 1.0f / std::tan(0.5f * fovYRadians);
		projectionMatrix = nvmath::mat4f_zero;
		projectionMatrix.set_row(0, nvmath::vec4f(f / aspectRatio, 0.0f, 0.0f, 0.0f));
		projectionMatrix.set_row(1, nvmath::vec4f(0.0f, f, 0.0f, 0.0f));
		projectionMatrix.set_row(2, nvmath::vec4f(0.0f, 0.0f, -zFar / (zNear - zFar), zNear * zFar / (zNear - zFar)));
		projectionMatrix.set_row(3, nvmath::vec4f(0.0f, 0.0f, 1.0f, 0.0f));

		projectionViewMatrix = projectionMatrix * viewMatrix;
		inverseViewMatrix = nvmath::invert(viewMatrix);
	}
};
