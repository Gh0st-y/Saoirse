// Main render pass and the geometry/lighting (deferred G-buffer) pipelines
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"
#include "ApplicationInternal.h"

// --- Create Render Pass ---
void Application::createRenderPass() {
    // --- Attachment 0: G-buffer position ---
    VkAttachmentDescription positionAttachment{};
    positionAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    positionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    positionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    positionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;   // only needed within this pass, not after
    positionAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    positionAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    positionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    positionAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // --- Attachment 1: G-buffer normal ---
    VkAttachmentDescription normalAttachment = positionAttachment;   // same settings, different format already matches

    // --- Attachment 2: G-buffer albedo ---
    VkAttachmentDescription albedoAttachment = positionAttachment;
    albedoAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;

    // --- Attachment 3: depth ---
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // --- Attachment 4: final swapchain color ---
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // this one DOES need to persist — it's what gets presented
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    std::array<VkAttachmentDescription, 5> attachments = {
        positionAttachment, normalAttachment, albedoAttachment, depthAttachment, colorAttachment
    };

    // ----- Subpass 0: geometry pass -----
    VkAttachmentReference positionRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference normalRef{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference albedoRef{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    std::array<VkAttachmentReference, 3> geometryColorRefs = { positionRef, normalRef, albedoRef };

    VkSubpassDescription geometrySubpass{};
    geometrySubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    geometrySubpass.colorAttachmentCount = static_cast<uint32_t>(geometryColorRefs.size());
    geometrySubpass.pColorAttachments = geometryColorRefs.data();
    geometrySubpass.pDepthStencilAttachment = &depthRef;

    // ----- Subpass 1: lighting pass -----
    VkAttachmentReference swapchainColorRef{ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    // input attachments: subpass 1 reads what subpass 0 wrote
    std::array<VkAttachmentReference, 3> lightingInputRefs = {
        VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
    };

    VkSubpassDescription lightingSubpass{};
    lightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    lightingSubpass.colorAttachmentCount = 1;
    lightingSubpass.pColorAttachments = &swapchainColorRef;
    lightingSubpass.inputAttachmentCount = static_cast<uint32_t>(lightingInputRefs.size());
    lightingSubpass.pInputAttachments = lightingInputRefs.data();

    std::array<VkSubpassDescription, 2> subpasses = { geometrySubpass, lightingSubpass };

    // ----- Subpass dependencies -----
    std::array<VkSubpassDependency, 2> dependencies{};

    // external -> geometry subpass: wait for previous frame's reads to finish
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // geometry subpass -> lighting subpass: lighting must wait for geometry writes before reading them
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(vulkanDevice.getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}


// --- Create Shader Module ---
VkShaderModule Application::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(vulkanDevice.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

// --- Create Geometry Pipeline ---
void Application::createGeometryPipeline() {
    auto vertShaderCode = readFile("shaders/geometry.vert.spv");
    auto fragShaderCode = readFile("shaders/geometry.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

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
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
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
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    //rasterizer.depthBiasConstantFactor = 
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;   // matches your earlier fix
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // --- THREE color blend attachments now, one per G-buffer output ---
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachments = {
        blendAttachment, blendAttachment, blendAttachment
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    colorBlending.pAttachments = blendAttachments.data();

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    std::array<VkDescriptorSetLayout, 2> setLayouts = { frameDescriptorSetLayout, objectDescriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();

    if (vkCreatePipelineLayout(vulkanDevice.getDevice(), &pipelineLayoutInfo, nullptr, &geometryPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create geometry pipeline layout!");
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
    pipelineInfo.layout = geometryPipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;   // <-- KEY: targets subpass 0, the geometry subpass

    if (vkCreateGraphicsPipelines(vulkanDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &geometryPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create geometry pipeline!");
    }

    vkDestroyShaderModule(vulkanDevice.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(vulkanDevice.getDevice(), vertShaderModule, nullptr);
}


// --- Create Lighting Pipeline ---
void Application::createLightingPipeline() {
    auto vertShaderCode = readFile("shaders/lighting.vert.spv");
    auto fragShaderCode = readFile("shaders/lighting.frag.spv");

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

    // No vertex input at all — positions are hardcoded in the shader
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
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
    rasterizer.cullMode = VK_CULL_MODE_NONE;   // <-- full-screen triangle, no culling wanted
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;    // <-- no depth testing needed
    depthStencil.depthWriteEnable = VK_FALSE;

    std::array<VkDescriptorSetLayout, 2> setLayouts = { frameDescriptorSetLayout, gBufferDescriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();

    if (vkCreatePipelineLayout(vulkanDevice.getDevice(), &pipelineLayoutInfo, nullptr, &lightingPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create lighting pipeline layout!");
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
    pipelineInfo.layout = lightingPipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 1;   // <-- KEY: targets subpass 1, the lighting subpass

    if (vkCreateGraphicsPipelines(vulkanDevice.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &lightingPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create lighting pipeline!");
    }

    vkDestroyShaderModule(vulkanDevice.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(vulkanDevice.getDevice(), vertShaderModule, nullptr);
}


// --- Create GBuffer Resources ---
void Application::createGBufferResources() {
    vulkanDevice.createImage(swapChainExtent.width, swapChainExtent.height, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferPosition, gBufferPositionMemory);

    vulkanDevice.createImage(swapChainExtent.width, swapChainExtent.height, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferNormal, gBufferNormalMemory);

    vulkanDevice.createImage(swapChainExtent.width, swapChainExtent.height, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferAlbedo, gBufferAlbedoMemory);

    auto makeView = [&](VkImage image, VkFormat format, VkImageView& outView) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vulkanDevice.getDevice(), &viewInfo, nullptr, &outView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create G-buffer image view!");
        }
        };

    makeView(gBufferPosition, VK_FORMAT_R16G16B16A16_SFLOAT, gBufferPositionView);
    makeView(gBufferNormal, VK_FORMAT_R16G16B16A16_SFLOAT, gBufferNormalView);
    makeView(gBufferAlbedo, VK_FORMAT_R8G8B8A8_UNORM, gBufferAlbedoView);
}


// --- Create GBuffer Descriptor Set Layout ---
void Application::createGBufferDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding positionBinding{};
    positionBinding.binding = 0;
    positionBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    positionBinding.descriptorCount = 1;
    positionBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding normalBinding = positionBinding;
    normalBinding.binding = 1;

    VkDescriptorSetLayoutBinding albedoBinding = positionBinding;
    albedoBinding.binding = 2;

    VkDescriptorSetLayoutBinding shadowMapBinding{};
    shadowMapBinding.binding = 3;
    shadowMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowMapBinding.descriptorCount = 4;
    shadowMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding spotShadowBinding{};
    spotShadowBinding.binding = 4;
    spotShadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    spotShadowBinding.descriptorCount = MAX_SPOT_LIGHTS;   // array of 4
    spotShadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding pointShadowBinding{};
    pointShadowBinding.binding = 5;
    pointShadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pointShadowBinding.descriptorCount = MAX_POINT_LIGHTS;
    pointShadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 6> bindings = {
        positionBinding, normalBinding, albedoBinding,
        shadowMapBinding, spotShadowBinding, pointShadowBinding
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vulkanDevice.getDevice(), &layoutInfo, nullptr, &gBufferDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create G-buffer descriptor set layout!");
    }
}


// --- Create GBuffer Descriptor Set ---
void Application::createGBufferDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &gBufferDescriptorSetLayout;

    if (vkAllocateDescriptorSets(vulkanDevice.getDevice(), &allocInfo, &gBufferDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate G-buffer descriptor set!");
    }

    VkDescriptorImageInfo positionInfo{};
    positionInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    positionInfo.imageView = gBufferPositionView;

    VkDescriptorImageInfo normalInfo = positionInfo;
    normalInfo.imageView = gBufferNormalView;

    VkDescriptorImageInfo albedoInfo = positionInfo;
    albedoInfo.imageView = gBufferAlbedoView;

    std::array<VkDescriptorImageInfo, 4> shadowMapInfos{};
    for (int i = 0; i < 4; i++) {
        shadowMapInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        shadowMapInfos[i].imageView = cascadeImageViews[i];
        shadowMapInfos[i].sampler = shadowMapSampler;
    }

    std::array<VkDescriptorImageInfo, MAX_SPOT_LIGHTS> spotShadowInfos{};
    for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
        spotShadowInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        spotShadowInfos[i].imageView = spotShadowViews[i];
        spotShadowInfos[i].sampler = spotShadowSampler;
    }

    std::array<VkDescriptorImageInfo, MAX_POINT_LIGHTS> pointShadowInfos{};
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        pointShadowInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pointShadowInfos[i].imageView = pointShadowCubemapViews[i];   // full cubemap view
        pointShadowInfos[i].sampler = pointShadowSampler;
    }

    std::array<VkWriteDescriptorSet, 6> writes{};
    writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writes[0].dstSet = gBufferDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &positionInfo;

    writes[1] = writes[0];
    writes[1].dstBinding = 1;
    writes[1].pImageInfo = &normalInfo;

    writes[2] = writes[0];
    writes[2].dstBinding = 2;
    writes[2].pImageInfo = &albedoInfo;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = gBufferDescriptorSet;
    writes[3].dstBinding = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 4;
    writes[3].pImageInfo = shadowMapInfos.data();

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = gBufferDescriptorSet;
    writes[4].dstBinding = 4;
    writes[4].dstArrayElement = 0;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].descriptorCount = MAX_SPOT_LIGHTS;
    writes[4].pImageInfo = spotShadowInfos.data();

    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = gBufferDescriptorSet;
    writes[5].dstBinding = 5;
    writes[5].dstArrayElement = 0;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[5].descriptorCount = MAX_POINT_LIGHTS;
    writes[5].pImageInfo = pointShadowInfos.data();

    vkUpdateDescriptorSets(vulkanDevice.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
