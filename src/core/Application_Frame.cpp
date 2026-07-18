// Command pool/buffer setup, command buffer recording, sync objects, and the main per-frame draw call
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"

// --- Create Command Buffer ---
void Application::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vulkanDevice.getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(vulkanDevice.getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}


// --- Record Command Buffer ---
void Application::recordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, uint32_t frameIndex) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;

    // 5 attachments now need clear values: position, normal, albedo, depth, swapchain color
    std::array<VkClearValue, 5> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };   // position
    clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };   // normal
    clearValues[2].color = { {0.0f, 0.0f, 0.0f, 1.0f} };   // albedo
    clearValues[3].depthStencil = { 1.0f, 0 };               // depth
    clearValues[4].color = { {0.0f, 0.0f, 0.0f, 1.0f} };   // final swapchain color

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    // ===== SUBPASS 0: Geometry pass =====
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometryPipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometryPipelineLayout,
        0, 1, &frameDescriptorSets[frameIndex], 0, nullptr);

    for (auto& obj : scene.gameObjects) {
        glm::vec3 worldCenter = obj->getWorldBoundingSphereCenter();
        float worldRadius = obj->getWorldBoundingSphereRadius();

        if (!currentFrustum.isSphereVisible(worldCenter, worldRadius)) {
            continue;
        }

        VkBuffer vertexBuffers[] = { obj->mesh->vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, obj->mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometryPipelineLayout,
            1, 1, &obj->descriptorSets[frameIndex], 0, nullptr);

        vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(obj->mesh->indices.size()), 1, 0, 0, 0);
    }

    // ===== Transition to subpass 1 =====
    vkCmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

    // ===== SUBPASS 1: Lighting pass =====
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipelineLayout,
        0, 1, &frameDescriptorSets[frameIndex], 0, nullptr);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipelineLayout,
        1, 1, &gBufferDescriptorSet, 0, nullptr);

    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);   // the hardcoded full-screen triangle — no vertex/index buffer needed

    vkCmdEndRenderPass(cmdBuffer);
}


// --- Create Sync Objects ---
void Application::createSyncObjects() {
    renderFinishedSemaphores.resize(swapChainImages.size());
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
       if (vkCreateSemaphore(vulkanDevice.getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vulkanDevice.getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects!");
        }
    }

    for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
        if (vkCreateSemaphore(vulkanDevice.getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects!");
        }
    }
}

// --- Draw Frame ---
void Application::drawFrame() {
    vkWaitForFences(vulkanDevice.getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(vulkanDevice.getDevice(), 1, &inFlightFences[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(vulkanDevice.getDevice(), swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    updateCascades();
    updateSpotShadowMatrices();
    updatePointShadowMatrices();

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix(swapChainExtent.width / (float)swapChainExtent.height);
    currentFrustum = Frustum::fromViewProjection(proj * view);

    updateFrameUniformBuffer(currentFrame);
    for (auto& obj : scene.gameObjects) {                
        updateGameObjectUniformBuffer(*obj, currentFrame);
    }

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    recordShadowPass(commandBuffers[currentFrame], currentFrame);
    recordSpotShadowPass(commandBuffers[currentFrame], currentFrame);
    recordPointShadowPass(commandBuffers[currentFrame], currentFrame);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex, currentFrame);

    if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    // ===== CHANGED: signal the semaphore belonging to THIS image index =====
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(vulkanDevice.getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;   // same one we just signaled

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(vulkanDevice.getPresentQueue(), &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
