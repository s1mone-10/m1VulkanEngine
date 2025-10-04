#include "Camera.hpp"

namespace m1
{
	void Camera::setPerspectiveProjection(float aspectRatio, float fov, float nearPlane, float farPlane)
	{
		_projectionType = ProjectionType::Perspective;
		_aspectRatio = aspectRatio;
		_fov = fov;
		_nearPlane = nearPlane;
		_farPlane = farPlane;
		updateProjectionMatrix();
	}

	void Camera::setOrthographicProjection(float left, float right, float bottom, float top, float nearPlane, float farPlane)
	{
		_projectionType = ProjectionType::Orthographic;
		_left = left;
		_right = right;
		_bottom = bottom;
		_top = top;
		_nearPlane = nearPlane;
		_farPlane = farPlane;
		updateProjectionMatrix();
	}

	void Camera::setViewDirection(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& up)
	{
		_position = position;
		_target = position + direction;
		_up = up;
		updateViewMatrix();
	}

	void Camera::setViewTarget(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up)
	{
		_position = position;
		_target = target;
		_up = up;
		updateViewMatrix();
	}

	void Camera::setAspectRatio(float aspectRatio)
	{
		_aspectRatio = aspectRatio;
		if (_projectionType == ProjectionType::Perspective)
			updateProjectionMatrix();
	}

	void Camera::updateViewMatrix()
	{
		_viewMatrix = glm::lookAt(_position, _target, _up);
	}

	void Camera::updateProjectionMatrix()
	{
		if (_projectionType == ProjectionType::Perspective)
		{
			_projectionMatrix = glm::perspective(glm::radians(_fov), _aspectRatio, _nearPlane, _farPlane);
		}
		else if (_projectionType == ProjectionType::Orthographic)
		{
			_projectionMatrix = glm::ortho(_left, _right, _bottom, _top, _nearPlane, _farPlane);
		}

		// flip the sign of Y scaling factor because GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
		_projectionMatrix[1][1] *= -1;
	}
} // namespace m1