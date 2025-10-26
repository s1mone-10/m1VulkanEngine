#pragma once

// libs
#include "glm_config.hpp"

namespace m1
{
	class Camera
	{
	public:
		enum class ProjectionType
		{
			Perspective,
			Orthographic
		};

		Camera();
		void setPerspectiveProjection(float aspectRatio, float fov = 45.0f, float nearPlane = 0.1f, float farPlane = 100.0f);
		void setOrthographicProjection(float left, float right, float bottom, float top, float nearPlane = 0.1f, float farPlane = 100.0f);
		void setViewDirection(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& up = glm::vec3(0.0f, -1.0f, 0.0f));
		void setViewTarget(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, -1.0f, 0.0f));
		void setPosition(const glm::vec3& pos) { _position = pos; updateViewMatrix(); }
		void setTarget(const glm::vec3& target) { _target = target; updateViewMatrix(); }
		void setUp(const glm::vec3& up) { _up = up; updateViewMatrix(); }
		void setAspectRatio(float aspectRatio);
		const glm::mat4& getViewMatrix() const { return _viewMatrix; }
		const glm::mat4& getProjectionMatrix() const { return _projectionMatrix; }
		ProjectionType getProjectionType() const { return _projectionType; }
		void setProjectionType(ProjectionType projectionType) { _projectionType = projectionType; updateProjectionMatrix(); }

		// movement and rotation
		void moveForward(float delta);
		void moveRight(float delta);
		void moveUp(float delta);
		void orbitHorizontal(float delta);
		void orbitVertical(float delta);
		void zoom(float factor);

	private:
		void updateViewMatrix();
		void updateProjectionMatrix();

		ProjectionType _projectionType{ ProjectionType::Perspective };
		float _nearPlane = 0.1f;
		float _farPlane = 100.0f;
		
		// View 
		glm::vec3 _position{0.0f, 0.0f, 5.0f};
		glm::vec3 _target{ 0.0f, 0.0f, 0.0f };
		glm::vec3 _up{ 0.0f, -1.0f, 0.0f };

		// Perspective
		float _aspectRatio;
		float _fov = 45.0f;

		// Ortho
		float _left = -1.0f, _right = 1.0f;
		float _bottom = -1.0f, _top = 1.0f;
		float _zoomfactor = 1.0f;

		// Matrices
		glm::mat4 _viewMatrix{1.0f};
		glm::mat4 _projectionMatrix{1.0f};

		// Camera speed
		float _cameraSpeed = 2.5f;
	};
} // namespace m1