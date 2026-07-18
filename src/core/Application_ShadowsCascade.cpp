// Cascaded shadow maps (directional light): resources, pipeline, matrix updates, recording
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"
#include "ApplicationInternal.h"


// --- Create Shadow Render Pass ---
void Application::createShadowRenderPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // <-- must persist, since we sample it afterward
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;   // ready for sampling afterward

    VkAttachmentReference depthRef{ 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> dependencies{};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // NEW: ensures the depth write in THIS render pass is visible before
    // the lighting pass's fragment shader samples it later in the same frame
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(vulkanDevice.getDevice(), &renderPassInfo, nullptr, &shadowRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shadow render pass!");
    }
}


// --- Create Shadow Resources ---
void Application::createShadowResources() {
    for (int i = 0; i < 4; i++) {
        uint32_t size = CASCADE_SIZES[i];

        vulkanDevice.createImage(size, size, findDepthFormat(), VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, cascadeImages[i], cascadeImageMemories[i]);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = cascadeImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = findDepthFormat();
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vulkanDevice.getDevice(), &viewInfo, nullptr, &cascadeImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create cascade image view!");
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = shadowRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &cascadeImageViews[i];
        framebufferInfo.width = size;
        framebufferInfo.height = size;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanDevice.getDevice(), &framebufferInfo, nullptr, &cascadeFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create cascade framebuffer!");
        }
    }

    // Sampler — same settings as Stage 1, created once, reused for all 4 cascades
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

    if (vkCreateSampler(vulkanDevice.getDevice(), &samplerInfo, nullptr, &shadowMapSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shadow map sampler!");
    }
}


// --- Create Shadow Pipeline ---
void Application::createShadowPipeline() {
    auto vertShaderCode = readFile("shaders/shadow.vert.spv");
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertShaderModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage };   // <-- only ONE stage, no fragment shader at all

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    std::vector<VkDynamicState> dynamicStates = { 
        VK_DYNAMIC_STATE_VIEWPORT, 
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;   // avoid losing shadow casters due to winding/culling quirks
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;        // <-- helps reduce shadow acne, tuned in Stage 5
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendStateCreateInfo colorBlending{};   // no color attachments — this can stay mostly default
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 0;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &objectDescriptorSetLayout;   // only need the object's model matrix
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(vulkanDevice.getDevice(), &pipelineLayoutInfo, nullptr, &shadowPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shadow pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 1;   // <-- just the vertex stage
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = shadowPipelineLayout;
    pipelineInfo.renderPass = shadowRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(vulkanDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &shadowPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shadow pipeline!");
    }

    vkDestroyShaderModule(vulkanDevice.getDevice(), vertShaderModule, nullptr);
}


// --- Update Shadow Matrix ---
void Application::updateShadowMatrix() {
    // Find the sun light (first directional light in the scene, for now)
    glm::vec3 lightDir = glm::vec3(0.0f, 0.0f, -1.0f);
    for (const auto& light : scene.lights) {
        if (light.type == LightType::Directional) {
            lightDir = light.getWorldDirection();
            break;
        }
    }

    // Temporary fixed bounds — Stage 2 will replace this with proper per-cascade frustum fitting
    float orthoSize = 30.0f;
    glm::vec3 sceneCenter = glm::vec3(2.0f, 0.0f, 0.0f);   // roughly between our two monkeys, hardcoded for now

    glm::mat4 lightView = glm::lookAt(sceneCenter - lightDir * 10.0f, sceneCenter, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 30.0f);
    lightProj[1][1] *= -1;   // same Vulkan Y-flip as our regular projection matrix

    lightSpaceMatrix = lightProj * lightView;
}


// --- Update Cascades ---
void Application::updateCascades() {
    float nearClip = 0.1f;
    float farClip = 100.0f;
    auto splitDepths = computeCascadeSplits(nearClip, farClip);

    glm::vec3 lightDir = glm::vec3(0.0f, 0.0f, -1.0f);
    for (const auto& light : scene.lights) {
        if (light.type == LightType::Directional) {
            lightDir = light.getWorldDirection();
            break;
        }
    }

    float lastSplitDist = nearClip;
    float aspect = swapChainExtent.width / (float)swapChainExtent.height;

    for (int cascadeIdx = 0; cascadeIdx < 4; cascadeIdx++) {
        float splitDist = splitDepths[cascadeIdx];

        // Camera projection covering just this cascade's depth slice
        glm::mat4 cascadeProj = glm::perspective(glm::radians(camera.fov), aspect, lastSplitDist, splitDist);
        cascadeProj[1][1] *= -1;

        auto corners = getFrustumCornersWorldSpace(cascadeProj, camera.getViewMatrix());

        glm::vec3 center(0.0f);
        for (const auto& corner : corners) center += corner;
        center /= 8.0f;

        glm::mat4 lightView = glm::lookAt(center - lightDir * 50.0f, center, glm::vec3(0.0f, 0.0f, 1.0f));

        glm::vec3 minBounds(std::numeric_limits<float>::max());
        glm::vec3 maxBounds(std::numeric_limits<float>::lowest());
        for (const auto& corner : corners) {
            glm::vec3 lightSpaceCorner = glm::vec3(lightView * glm::vec4(corner, 1.0f));
            minBounds = glm::min(minBounds, lightSpaceCorner);
            maxBounds = glm::max(maxBounds, lightSpaceCorner);
        }

        // Add a small margin to avoid clipping geometry right at the boundary
        float margin = 5.0f;

        float nearPlane = -maxBounds.z - 200.0f;   // less negative view-Z = closer to light = smaller distance
        float farPlane = -minBounds.z + margin;     // more negative view-Z = farther from light = larger distance

        glm::mat4 lightProj = glm::ortho(
            minBounds.x - margin, maxBounds.x + margin,
            minBounds.y - margin, maxBounds.y + margin,
            nearPlane, farPlane
        );
        lightProj[1][1] *= -1;

        cascades[cascadeIdx].viewProj = lightProj * lightView;
        cascades[cascadeIdx].splitDepth = splitDist;

        lastSplitDist = splitDist;
    }
}


// --- Compute Cascade Splits ---
std::array<float, 4> Application::computeCascadeSplits(float nearClip, float farClip) {
    std::array<float, 4> splits;
    float lambda = 0.5f; // blend factor between linear and logarithmic

    for (int i = 0; i < 4; i++) {
        float p = (i + 1) / 4.0f;
        float logSplit = nearClip * std::pow(farClip / nearClip, p);
        float linSplit = nearClip + (farClip - nearClip) * p;
        splits[i] = lambda * logSplit + (1.0f - lambda) * linSplit;
    }

    return splits;
}


// --- Get Frustum Corners World Space ---
std::array<glm::vec3, 8> Application::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
    glm::mat4 invViewProj = glm::inverse(proj * view);

    std::array<glm::vec3, 8> corners;
    int i = 0;
    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            for (int z = 0; z < 2; z++) {
                glm::vec4 pt = invViewProj * glm::vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    (float)z,
                    1.0f
                );
                corners[i++] = glm::vec3(pt) / pt.w;
            }
        }
    }
    return corners;
}


// --- Record Shadow Pass ---
void Application::recordShadowPass(VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
    for (int cascadeIdx = 0; cascadeIdx < 4; cascadeIdx++) {
        uint32_t size = CASCADE_SIZES[cascadeIdx];

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = shadowRenderPass;
        renderPassInfo.framebuffer = cascadeFramebuffers[cascadeIdx];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { size, size };

        VkClearValue clearValue{};
        clearValue.depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{ 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor{ {0, 0}, {size, size} };
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);

        // Per-cascade bias — near cascades need less bias (higher resolution = less texel size error)
        // far cascades need more. These values are a starting point; tune by eye.
        const float biasConstant[4] = { 0.5f,  1.0f,  1.5f,  2.0f };
        const float biasSlope[4] = { 0.75f, 1.25f, 1.75f, 2.25f };

        vkCmdSetDepthBias(
            cmdBuffer,
            biasConstant[cascadeIdx],
            0.0f,                       // clamp (0 = unclamped, fine for now)
            biasSlope[cascadeIdx]
        );

        vkCmdPushConstants(cmdBuffer, shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(glm::mat4), &cascades[cascadeIdx].viewProj);   // <-- per-cascade matrix

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
    }
}
