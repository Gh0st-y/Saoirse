#pragma once

#include "GameObject.h"
#include "Light.h"
#include <vector>
#include <memory>

class Scene {
public:
    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<Light> lights;

    GameObject& addGameObject(Mesh* mesh, Texture* texture) {
        auto obj = std::make_unique<GameObject>();
        obj->mesh = mesh;
        obj->texture = texture;
        gameObjects.push_back(std::move(obj));
        return *gameObjects.back();
    }

    Light& addLight(LightType type) {
        lights.push_back(Light{});
        Light& light = lights.back();
        light.type = type;
        return light;
    }
};