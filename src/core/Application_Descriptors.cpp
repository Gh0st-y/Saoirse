// Descriptor set layouts, the per-frame uniform buffer, and the descriptor pool
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"


// --- Create Frame Descriptor Set Layout ---
void Application::createFrameDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(vulkanDevice.getDevice(), &layoutInfo, nullptr, &frameDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create frame descriptor set layout!");
    }
}


// --- Create Object Descriptor Set Layout ---
void Application::createObjectDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding normalMapLayoutBinding{};
    normalMapLayoutBinding.binding = 2;
    normalMapLayoutBinding.descriptorCount = 1;
    normalMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normalMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = { uboLayoutBinding, samplerLayoutBinding, normalMapLayoutBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vulkanDevice.getDevice(), &layoutInfo, nullptr, &objectDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create object descriptor set layout!");
    }
}


// --- Create Frame Uniform Buffer ---
void Application::createFrameUniformBuffer() {
    VkDeviceSize bufferSize = sizeof(FrameUBO);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vulkanDevice.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        frameUniformBuffers[i], frameUniformBufferMemories[i]);
        vkMapMemory(vulkanDevice.getDevice(), frameUniformBufferMemories[i], 0, bufferSize, 0, &frameUniformBuffersMapped[i]);
    }
}


// --- Update Frame Uniform Buffer ---
void Application::updateFrameUniformBuffer(uint32_t frameIndex) {
    FrameUBO frame{};
    frame.view = camera.getViewMatrix();
    frame.proj = camera.getProjectionMatrix(swapChainExtent.width / (float)swapChainExtent.height);
    for (int i = 0; i < 4; i++) {
        frame.cascadeViewProj[i] = cascades[i].viewProj;
    }
    frame.cascadeSplits = glm::vec4(cascades[0].splitDepth, cascades[1].splitDepth, cascades[2].splitDepth, cascades[3].splitDepth);

    int dirCount = 0, pointCount = 0, spotCount = 0;

    for (const auto& light : scene.lights) {
        switch (light.type) {
        case LightType::Directional:
            if (dirCount < MAX_DIR_LIGHTS) {
                frame.dirLights[dirCount].direction = light.getWorldDirection();
                frame.dirLights[dirCount].color = light.color;
                dirCount++;
            }
            break;

        case LightType::Point:
            if (pointCount < MAX_POINT_LIGHTS) {
                frame.pointLights[pointCount].position = light.getWorldPosition();
                frame.pointLights[pointCount].color = light.color;
                frame.pointLights[pointCount].attenuation = light.attenuation;
                pointCount++;
            }
            break;

        case LightType::Spot:
            if (spotCount < MAX_SPOT_LIGHTS) {
                frame.spotLights[spotCount].position = light.getWorldPosition();
                frame.spotLights[spotCount].direction = light.getWorldDirection();
                frame.spotLights[spotCount].color = light.color;
                frame.spotLights[spotCount].attenuation = light.attenuation;
                frame.spotLights[spotCount].cutOff = light.getCutOff();
                spotCount++;
            }
            break;
        }
    }

    for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
        frame.spotLightMatrices[i] = spotLightMatrices[i];
    }

    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        for (int face = 0; face < 6; face++) {
            frame.pointLightMatrices[i][face] = pointLightMatrices[i][face];
        }
    }
    frame.pointLightFarPlane = glm::vec4(100.0f, 0.0f, 0.0f, 0.0f);   // matches the 100.0f far plane in updatePointShadowMatrices()

    frame.viewPosAndCounts = glm::vec4(camera.position, 0.0f);
    frame.lightCounts = glm::ivec4(dirCount, pointCount, spotCount, 0);

    memcpy(frameUniformBuffersMapped[frameIndex], &frame, sizeof(frame));
}


// --- Create Frame Descriptor Set ---
void Application::createFrameDescriptorSet() {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &frameDescriptorSetLayout;
        if (vkAllocateDescriptorSets(vulkanDevice.getDevice(), &allocInfo, &frameDescriptorSets[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate frame descriptor set!");
        }
        
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = frameUniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(FrameUBO);
        
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = frameDescriptorSets[i];
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;
        
        vkUpdateDescriptorSets(vulkanDevice.getDevice(), 1, &write, 0, nullptr);
    }
}


// --- Create Descriptor Pool ---
void Application::createDescriptorPool() {
    const uint32_t maxObjects = 100;

    std::array<VkDescriptorPoolSize, 3> poolSizes{};   // <-- CHANGED: now 3 types
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = maxObjects + 1;   // per-object UBOs + the one frame UBO

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = maxObjects * 2 + MAX_SPOT_LIGHTS + MAX_POINT_LIGHTS;         // per-object textures only

    poolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;   // <-- NEW
    poolSizes[2].descriptorCount = 3;                            // position + normal + albedo

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxObjects + 2;   // <-- CHANGED: +1 frame set, +1 gBuffer set

    if (vkCreateDescriptorPool(vulkanDevice.getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}
