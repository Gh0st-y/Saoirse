// Point light shadow cubemaps: pipeline, resources, matrix updates, recording
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"
#include "ApplicationInternal.h"


// --- Create Point Shadow Pipeline ---
void Application::createPointShadowPipeline() {
    auto vertShaderCode = readFile("shaders/shadowPoint.vert.spv");
    auto fragShaderCode = readFile("shaders/shadowPoint.frag.spv");
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertShaderModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragShaderModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

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
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;
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

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 0;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);   // matrix + lightPos/farPlane

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &objectDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(vulkanDevice.getDevice(), &pipelineLayoutInfo, nullptr, &pointShadowPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create point shadow pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = pointShadowPipelineLayout;
    pipelineInfo.renderPass = pointShadowRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(vulkanDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pointShadowPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create point shadow pipeline!");
    }

    vkDestroyShaderModule(vulkanDevice.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(vulkanDevice.getDevice(), vertShaderModule, nullptr);
}


// --- Create Point Shadow Resources ---
void Application::createPointShadowResources() {
    // Create the render pass first — same as shadowRenderPass, separate instance for clarity
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

    if (vkCreateRenderPass(vulkanDevice.getDevice(), &renderPassInfo, nullptr, &pointShadowRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create point shadow render pass!");
    }

    // Create cubemap images, views, and framebuffers
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        // Create the cubemap image — VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT + 6 arrayLayers
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = { POINT_SHADOW_SIZE, POINT_SHADOW_SIZE, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 6;   // <-- 6 faces
        imageInfo.format = findDepthFormat();
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;   // <-- required for cube map sampling

        if (vkCreateImage(vulkanDevice.getDevice(), &imageInfo, nullptr, &pointShadowCubemaps[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create point shadow cubemap!");
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(vulkanDevice.getDevice(), pointShadowCubemaps[i], &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = vulkanDevice.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(vulkanDevice.getDevice(), &allocInfo, nullptr, &pointShadowMemories[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate point shadow memory!");
        }
        vkBindImageMemory(vulkanDevice.getDevice(), pointShadowCubemaps[i], pointShadowMemories[i], 0);

        // Full cubemap view — used by the shader for sampling
        VkImageViewCreateInfo cubeViewInfo{};
        cubeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        cubeViewInfo.image = pointShadowCubemaps[i];
        cubeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;   // <-- cube view for sampling
        cubeViewInfo.format = findDepthFormat();
        cubeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        cubeViewInfo.subresourceRange.levelCount = 1;
        cubeViewInfo.subresourceRange.baseArrayLayer = 0;
        cubeViewInfo.subresourceRange.layerCount = 6;   // <-- all 6 faces

        if (vkCreateImageView(vulkanDevice.getDevice(), &cubeViewInfo, nullptr, &pointShadowCubemapViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create point shadow cubemap view!");
        }

        // Per-face views + framebuffers — used for rendering into individual faces
        for (int face = 0; face < 6; face++) {
            VkImageViewCreateInfo faceViewInfo{};
            faceViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            faceViewInfo.image = pointShadowCubemaps[i];
            faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;   // <-- 2D view of a single face
            faceViewInfo.format = findDepthFormat();
            faceViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            faceViewInfo.subresourceRange.levelCount = 1;
            faceViewInfo.subresourceRange.baseArrayLayer = face;   // <-- this specific face
            faceViewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(vulkanDevice.getDevice(), &faceViewInfo, nullptr, &pointShadowFaceViews[i][face]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create point shadow face view!");
            }

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = pointShadowRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &pointShadowFaceViews[i][face];
            framebufferInfo.width = POINT_SHADOW_SIZE;
            framebufferInfo.height = POINT_SHADOW_SIZE;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(vulkanDevice.getDevice(), &framebufferInfo, nullptr, &pointShadowFramebuffers[i][face]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create point shadow framebuffer!");
            }
        }
    }

    // Transition ALL faces of ALL cubemaps to SHADER_READ_ONLY_OPTIMAL upfront
    // so inactive/never-rendered faces don't stay in UNDEFINED layout
    VkCommandBuffer cmdBuffer = vulkanDevice.beginSingleTimeCommands();

    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = pointShadowCubemaps[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 6;   // all 6 faces in one barrier
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vulkanDevice.endSingleTimeCommands(cmdBuffer);

    // Sampler for cubemap sampling — no comparison sampler needed here,
    // we'll do manual depth comparison in the shader (explained below)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_FALSE;   // <-- manual comparison in shader for cubemaps

    if (vkCreateSampler(vulkanDevice.getDevice(), &samplerInfo, nullptr, &pointShadowSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create point shadow sampler!");
    }
}


// --- Update Point Shadow Matrices ---
void Application::updatePointShadowMatrices() {
    // Standard cubemap face directions and up vectors
    // These match Vulkan's cube face ordering: +X, -X, +Y, -Y, +Z, -Z
    const glm::vec3 faceDirections[6] = {
        { 1.0f,  0.0f,  0.0f},   // +X
        {-1.0f,  0.0f,  0.0f},   // -X
        { 0.0f,  1.0f,  0.0f},   // +Y
        { 0.0f, -1.0f,  0.0f},   // -Y
        { 0.0f,  0.0f,  1.0f},   // +Z
        { 0.0f,  0.0f, -1.0f}    // -Z
    };

    const glm::vec3 faceUps[6] = {
        { 0.0f, -1.0f,  0.0f},   // +X up
        { 0.0f, -1.0f,  0.0f},   // -X up
        { 0.0f,  0.0f,  1.0f},   // +Y up
        { 0.0f,  0.0f, -1.0f},   // -Y up
        { 0.0f, -1.0f,  0.0f},   // +Z up
        { 0.0f, -1.0f,  0.0f}    // -Z up
    };

    // 90 degree FOV, square aspect ratio — covers exactly one cube face
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    //shadowProj[1][1] *= -1;   // Vulkan Y-flip

    int pointIdx = 0;
    for (const auto& light : scene.lights) {
        if (light.type != LightType::Point) continue;
        if (pointIdx >= MAX_POINT_LIGHTS) break;

        glm::vec3 pos = light.getWorldPosition();

        for (int face = 0; face < 6; face++) {
            glm::mat4 faceView = glm::lookAt(pos, pos + faceDirections[face], faceUps[face]);
            pointLightMatrices[pointIdx][face] = shadowProj * faceView;
        }

        pointIdx++;
    }
}


// --- Record Point Shadow Pass ---
void Application::recordPointShadowPass(VkCommandBuffer cmdBuffer, uint32_t frameIndex) {
    struct PointShadowPushConstants {
        glm::mat4 lightSpaceMatrix;
        glm::vec4 lightPosFarPlane;
    };

    int pointIdx = 0;
    for (const auto& light : scene.lights) {
        if (light.type != LightType::Point) continue;
        if (pointIdx >= MAX_POINT_LIGHTS) break;

        glm::vec3 pos = light.getWorldPosition();

        for (int face = 0; face < 6; face++) {
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = pointShadowRenderPass;
            renderPassInfo.framebuffer = pointShadowFramebuffers[pointIdx][face];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = { POINT_SHADOW_SIZE, POINT_SHADOW_SIZE };

            VkClearValue clearValue{};
            clearValue.depthStencil = { 1.0f, 0 };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{ 0.0f, 0.0f, (float)POINT_SHADOW_SIZE, (float)POINT_SHADOW_SIZE, 0.0f, 1.0f };
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

            VkRect2D scissor{ {0, 0}, {POINT_SHADOW_SIZE, POINT_SHADOW_SIZE} };
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pointShadowPipeline);
            vkCmdSetDepthBias(cmdBuffer, 1.0f, 0.0f, 1.5f);

            PointShadowPushConstants pc{ pointLightMatrices[pointIdx][face], glm::vec4(pos, 100.0f) };
            vkCmdPushConstants(cmdBuffer, pointShadowPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(pc), &pc);

            for (auto& obj : scene.gameObjects) {
                VkBuffer vertexBuffers[] = { obj->mesh->vertexBuffer };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(cmdBuffer, obj->mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pointShadowPipelineLayout,
                    0, 1, &obj->descriptorSets[frameIndex], 0, nullptr);

                vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(obj->mesh->indices.size()), 1, 0, 0, 0);
            }

            vkCmdEndRenderPass(cmdBuffer);
        }

        pointIdx++;
    }
}
