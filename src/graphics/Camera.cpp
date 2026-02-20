#include "Camera.hpp"
#include "Utils.hpp"

namespace m1
{
	Camera::Camera()
	{
		setViewTarget(glm::vec3(0.0f, -2.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		setPerspectiveProjection(1);
	}

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

	void Camera::moveForward(float delta)
	{
		glm::vec3 viewDir = glm::normalize(_target - _position);
		_position += viewDir * delta * _cameraSpeed;
		updateViewMatrix();
	}

	void Camera::moveRight(float delta)
	{
		glm::vec3 viewDir = glm::normalize(_target - _position);
		glm::vec3 right = glm::normalize(glm::cross(viewDir, _up));
		_position += right * delta * _cameraSpeed;
		_target += right * delta * _cameraSpeed;
		updateViewMatrix();
	}

	void Camera::moveUp(float delta)
	{
		_position += _up * delta * _cameraSpeed;
		_target += _up * delta * _cameraSpeed;
		updateViewMatrix();
	}

	void Camera::orbitHorizontal(float delta)
	{
		glm::vec3 viewDir = _position - _target;
		glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), glm::radians(delta * _cameraSpeed * 20), _up);
		viewDir = glm::vec3(yawRotation * glm::vec4(viewDir, 0.0f));
		_position = _target + viewDir;
		updateViewMatrix();
	}

	void Camera::orbitVertical(float delta)
	{
		glm::vec3 viewDir = _position - _target;
		glm::vec3 right = glm::normalize(glm::cross(glm::normalize(_target - _position), _up));
		glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), glm::radians(delta * _cameraSpeed * 20), right);
		viewDir = glm::vec3(pitchRotation * glm::vec4(viewDir, 0.0f));
		_position = _target + viewDir;
		updateViewMatrix();
	}

	void Camera::zoom(float factor)
	{
		glm::vec3 viewDir = glm::normalize(_target - _position);
		_position += viewDir * factor * _cameraSpeed;
		updateViewMatrix();

		_zoomfactor += factor;
		if(_projectionType == ProjectionType::Orthographic)
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
			_projectionMatrix = Utils::perspectiveProjection(glm::radians(_fov), _aspectRatio, _nearPlane, _farPlane);
		}
		else // Orthographic
		{
			_projectionMatrix = Utils::orthoProjection(_left / _zoomfactor, _right / _zoomfactor, _bottom / _zoomfactor, _top / _zoomfactor, _nearPlane, _farPlane);
		}
	}
} // namespace m1