// Spot light shadow maps: resources, matrix updates, recording
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"


// --- Create Spot Shadow Resources ---
void Application::createSpotShadowResources() {
    for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
        vulkanDevice.createImage(SPOT_SHADOW_SIZE, SPOT_SHADOW_SIZE, findDepthFormat(), VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, spotShadowImages[i], spotShadowMemories[i]);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = spotShadowImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = findDepthFormat();
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vulkanDevice.getDevice(), &viewInfo, nullptr, &spotShadowViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create spot shadow image view!");
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = shadowRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &spotShadowViews[i];
        framebufferInfo.width = SPOT_SHADOW_SIZE;
        framebufferInfo.height = SPOT_SHADOW_SIZE;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanDevice.getDevice(), &framebufferInfo, nullptr, &spotShadowFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create spot shadow framebuffer!");
        }
    }

    VkCommandBuffer cmdBuffer = vulkanDevice.beginSingleTimeCommands();

    for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = spotShadowImages[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vulkanDevice.endSingleTimeCommands(cmdBuffer);

    // Separate sampler from the CSM one — spot lights use perspective projection
    // so CLAMP_TO_BORDER still applies, but we keep it separate for clarity
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS;

    if (vkCreateSampler(vulkanDevice.getDevice(), &samplerInfo, nullptr, &spotShadowSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create spot shadow sampler!");
    }
}

// --- Update Spot Shadow Matrices ---
void Application::updateSpotShadowMatrices() {
    int spotIdx = 0;
    for (const auto& light : scene.lights) {
        if (light.type != LightType::Spot) continue;
        if (spotIdx >= MAX_SPOT_LIGHTS) break;

        glm::vec3 pos = light.getWorldPosition();
        glm::vec3 dir = light.getWorldDirection();

        // Build a stable "up" vector — avoid degenerate lookAt if dir is parallel to world up
        glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
        if (std::abs(glm::dot(dir, up)) > 0.99f) up = glm::vec3(0.0f, 1.0f, 0.0f);

        glm::mat4 lightView = glm::lookAt(pos, pos + dir, up);

        // Use outer cone angle as FOV — shadows must cover the full lit cone
        float fov = light.outerConeAngleDegrees * 2.0f;
        glm::mat4 lightProj = glm::perspective(glm::radians(fov), 1.0f, 0.1f, 100.0f);
        lightProj[1][1] *= -1;   // Vulkan Y-flip

        spotLightMatrices[spotIdx] = lightProj * lightView;
        spotIdx++;
    }
}


// --- Record Spot Shadow Pass ---
void Application::recordSpotShadowPass(VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
    int spotIdx = 0;
    for (const auto& light : scene.lights) {
        if (light.type != LightType::Spot) continue;
        if (spotIdx >= MAX_SPOT_LIGHTS) break;

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = shadowRenderPass;
        renderPassInfo.framebuffer = spotShadowFramebuffers[spotIdx];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { SPOT_SHADOW_SIZE, SPOT_SHADOW_SIZE };

        VkClearValue clearValue{};
        clearValue.depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{ 0.0f, 0.0f, (float)SPOT_SHADOW_SIZE, (float)SPOT_SHADOW_SIZE, 0.0f, 1.0f };
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, {SPOT_SHADOW_SIZE, SPOT_SHADOW_SIZE} };
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);

        // Flat bias for spot lights — no per-cascade variation needed
        vkCmdSetDepthBias(cmdBuffer, 1.0f, 0.0f, 1.5f);

        vkCmdPushConstants(cmdBuffer, shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(glm::mat4), &spotLightMatrices[spotIdx]);

        for (auto& obj : scene.gameObjects) {
            VkBuffer vertexBuffers[] = { obj->mesh->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmdBuffer, obj->mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipelineLayout,
                0, 1, &obj->descriptorSets[frameIndex], 0, nullptr);

            vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(obj->mesh->indices.size()), 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(cmdBuffer);
        spotIdx++;
    }
}
