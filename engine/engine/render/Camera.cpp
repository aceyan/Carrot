//
// Created by jglrxavpok on 06/12/2020.
//

#include "Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "engine/utils/Macros.h"

namespace Carrot {
    Camera::Camera(const glm::mat4& view, const glm::mat4& projection): type(ControlType::ViewProjection), viewMatrix(view), projectionMatrix(projection) {}

    Camera::Camera(float fov, float aspectRatio, float zNear, float zFar, glm::vec3 up): up(up), position(), target() {
        projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, zNear, zFar);
        projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
    }

    const glm::mat4& Camera::getProjectionMatrix() const {
        return projectionMatrix;
    }

    const glm::vec3& Camera::getUp() const {
        verify(type == ControlType::PoseAndLookAt, "Cannot get UP vector on non pose-and-look-at cameras!");
        return up;
    }

    const glm::mat4& Camera::computeViewMatrix() {
        switch (type) {
            case ControlType::ViewProjection:
                return viewMatrix;

            case ControlType::PoseAndLookAt:
                viewMatrix = glm::lookAt(position, target, up);
                return viewMatrix;

            default:
                TODO;
        }
    }

    const glm::vec3& Camera::getTarget() const {
        verify(type == ControlType::PoseAndLookAt, "Cannot get TARGET vector on non pose-and-look-at cameras!");
        return target;
    }

    const glm::vec3& Camera::getPosition() const {
        verify(type == ControlType::PoseAndLookAt, "Cannot get POSITION vector on non pose-and-look-at cameras!");
        return position;
    }

    glm::vec3& Camera::getTargetRef() {
        verify(type == ControlType::PoseAndLookAt, "Cannot get TARGET vector on non pose-and-look-at cameras!");
        return target;
    }

    glm::vec3& Camera::getPositionRef() {
        verify(type == ControlType::PoseAndLookAt, "Cannot get POSITION vector on non pose-and-look-at cameras!");
        return position;
    }

    glm::mat4& Camera::getViewMatrixRef() {
        verify(type == ControlType::ViewProjection, "Cannot get VIEW matrix on non view-projection cameras!");
        return viewMatrix;
    }

    glm::mat4& Camera::getProjectionMatrixRef() {
        return projectionMatrix;
    }

    void Camera::setTargetAndPosition(const glm::vec3& target, const glm::vec3& position) {
        type = ControlType::PoseAndLookAt;
        this->target = target;
        this->position = position;
    }

    void Camera::setViewProjection(const glm::mat4& view, const glm::mat4& projection) {
        type = ControlType::ViewProjection;
        this->viewMatrix = view;
        this->projectionMatrix = projection;
    }

    Camera& Camera::operator=(const Camera& toCopy) = default;
}