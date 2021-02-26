//
// Created by jglrxavpok on 05/12/2020.
//

#include <glm/gtc/matrix_transform.hpp>
#include "Unit.h"

Game::Unit::Unit(Unit::Type type, Carrot::AnimatedInstanceData& instanceData): instanceData(instanceData), type(type) {
    switch (type) {
        case Type::Blue:
            color = {0,0,1};
            break;
        case Type::Red:
            color = {1,0,0};
            break;
        case Type::Green:
            color = {0,1,0};
            break;
    }

    instanceData.animationTime = rand() / 10.0f;
    instanceData.animationIndex = rand() % 2;
}

void Game::Unit::update(float dt) {
    glm::vec3 direction = target-position;
    if(direction.length() > 0.01) {
        auto normalizedDirection = glm::vec3(direction.x / direction.length(), direction.y / direction.length(), direction.z / direction.length());
        float angle = atan2(normalizedDirection.y, normalizedDirection.x);
        rotation = glm::angleAxis(angle, glm::vec3(0,0,1));
        position += normalizedDirection * dt;
    }
    // TODO: move randomly

    instanceData.color = { color.r, color.g, color.b, 1.0f };
    instanceData.transform = getTransform();

    instanceData.animationTime += dt;
}

void Game::Unit::teleport(const glm::vec3& newPos) {
    position = newPos;
    target = newPos;
}

Game::Unit::Type Game::Unit::getType() const {
    return type;
}

void Game::Unit::moveTo(const glm::vec3& targetPosition) {
    target = targetPosition;
}

glm::vec3 Game::Unit::getPosition() const {
    return position;
}

glm::mat4 Game::Unit::getTransform() const {
    auto modelRotation = glm::rotate(rotation, 0.0f, glm::vec3(0,0,01));
    return glm::translate(glm::mat4(1.0f), position) * glm::toMat4(modelRotation);
}