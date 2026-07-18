// Application lifecycle, GLFW window, Vulkan instance/debug messenger/surface setup
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"
#include <iostream>

void Application::run() 
{
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

#pragma region Initialization

// --- Init Window ---
void Application::initWindow() 
{
    glfwInit();

    // Tell GLFW not to create an OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // we'll handle resizing properly later

    window = glfwCreateWindow(WIDTH, HEIGHT, "Saoirse", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

// --- Init Vulkan ---
void Application::initVulkan() {
    vulkanDevice.init(window);

    createSwapChain();
    createImageViews();

    createRenderPass();

    createFrameDescriptorSetLayout();   
    createObjectDescriptorSetLayout();  
    createGBufferDescriptorSetLayout();

    createShadowRenderPass();
    createShadowResources();
    createSpotShadowResources();
    createPointShadowResources();
    createShadowPipeline();
    createPointShadowPipeline();

    createGeometryPipeline();   
    createLightingPipeline();

    createDepthResources();
    createGBufferResources();
    createFramebuffers();

    createFrameUniformBuffer();

    createDefaultNormalMap();

    scene.gameObjects.reserve(10);   // safeguard against reference invalidation, per the note above

    Mesh* monkeyMesh = loadMesh("assets/models/monkey.obj");
    Mesh* floorMesh = loadMesh("assets/models/floor.obj");
    Texture* monkeyTexture = loadTexture("assets/textures/fabric.png");
    Texture* floorTexture = loadTexture("assets/textures/tile.png");
    Texture* monkeyNormal = loadTexture("assets/textures/fabric_N.png");
    // Texture* floorNormal = loadTexture("assets/textures/tile_N.png");

    Mesh* boxMesh = loadMesh("assets/models/box.obj");
    Texture* boxTexture = loadTexture("assets/textures/box.png");
    Texture* boxNormal = loadTexture("assets/textures/box_N.png");

    createDescriptorPool();   // must come before createGameObject(), since it allocates descriptor sets from this pool
    createFrameDescriptorSet();
    createGBufferDescriptorSet();

    GameObject& box = scene.addGameObject(boxMesh, boxTexture);
    box.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);
    box.position = glm::vec3(3.0f, 0.0f, 0.0f);
    box.normalMap = boxNormal;
    createGameObjectResources(box);

    GameObject& floor = scene.addGameObject(floorMesh, floorTexture);
    floor.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);
    floor.position = glm::vec3(0.0f, 0.0f, -3.0f);
    // floor.normalMap = floorNormal;
    createGameObjectResources(floor);

    GameObject& monkey = scene.addGameObject(monkeyMesh, monkeyTexture);
    monkey.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);
    monkey.position = glm::vec3(-3.0f, 0.0f, 0.0f);
    monkey.normalMap = monkeyNormal;
    createGameObjectResources(monkey);

    //GameObject& monkey2 = scene.addGameObject(monkeyMesh, monkeyTexture);
    //monkey2.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);
    //monkey2.position = glm::vec3(-4.5f, 0.0f, 0.0f);
    //createGameObjectResources(monkey2);

    //monkey2.setParent(&monkey);

    Light& sun = scene.addLight(LightType::Directional);
    sun.direction = glm::normalize(glm::vec3(-0.5f, -0.5f, -1.0f));
    sun.color = glm::vec3(0.3f, 0.3f, 0.35f);

    Light& lamp = scene.addLight(LightType::Point);
    lamp.position = glm::vec3(2.0f, 2.0f, 4.0f);
    lamp.color = glm::vec3(0.0f, 0.0f, 1.0f);
    lamp.attenuation = glm::vec3(1.0f, 0.09f, 0.032f);

    //Light& spotlight = scene.addLight(LightType::Spot);
    //spotlight.position = glm::vec3(-2.0f, -2.0f, 3.0f);
    //spotlight.direction = glm::normalize(glm::vec3(0.5f, 0.5f, -0.5f));
    //spotlight.color = glm::vec3(1.0f, 0.0f, 0.0f);
    //spotlight.attenuation = glm::vec3(1.0f, 0.09f, 0.032f);
    //spotlight.innerConeAngleDegrees = 12.5f;
    //spotlight.outerConeAngleDegrees = 17.5f;

    createCommandBuffers();
    createSyncObjects();
}

#pragma endregion

// --- Main Loop ---
void Application::mainLoop() 
{
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        glfwPollEvents();
        processInput(deltaTime);
        drawFrame();
    }

    vkDeviceWaitIdle(vulkanDevice.getDevice());
}

// --- Cleanup ---
void Application::cleanup()
{
    vkDestroyPipeline(vulkanDevice.getDevice(), shadowPipeline, nullptr);
    vkDestroyPipelineLayout(vulkanDevice.getDevice(), shadowPipelineLayout, nullptr);
    vkDestroyPipeline(vulkanDevice.getDevice(), pointShadowPipeline, nullptr);
    vkDestroyPipelineLayout(vulkanDevice.getDevice(), pointShadowPipelineLayout, nullptr);

    vkDestroyRenderPass(vulkanDevice.getDevice(), pointShadowRenderPass, nullptr);

    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        vkDestroyImageView(vulkanDevice.getDevice(), pointShadowCubemapViews[i], nullptr);
        for (int face = 0; face < 6; face++) {
            vkDestroyFramebuffer(vulkanDevice.getDevice(), pointShadowFramebuffers[i][face], nullptr);
            vkDestroyImageView(vulkanDevice.getDevice(), pointShadowFaceViews[i][face], nullptr);
        }
        vkDestroyImage(vulkanDevice.getDevice(), pointShadowCubemaps[i], nullptr);
        vkFreeMemory(vulkanDevice.getDevice(), pointShadowMemories[i], nullptr);
    }

    vkDestroySampler(vulkanDevice.getDevice(), pointShadowSampler, nullptr);

    for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
        vkDestroyFramebuffer(vulkanDevice.getDevice(), spotShadowFramebuffers[i], nullptr);
        vkDestroyImageView(vulkanDevice.getDevice(), spotShadowViews[i], nullptr);
        vkDestroyImage(vulkanDevice.getDevice(), spotShadowImages[i], nullptr);
        vkFreeMemory(vulkanDevice.getDevice(), spotShadowMemories[i], nullptr);
    }
    vkDestroySampler(vulkanDevice.getDevice(), spotShadowSampler, nullptr);

    for (auto& obj : scene.gameObjects) {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(vulkanDevice.getDevice(), obj->uniformBuffers[i], nullptr);
            vkFreeMemory(vulkanDevice.getDevice(), obj->uniformBufferMemories[i], nullptr);
        }
    }

    for (auto& mesh : meshes) destroyMesh(mesh.get());
    for (auto& texture : textures) destroyTexture(texture.get());

    vkDestroyDescriptorPool(vulkanDevice.getDevice(), descriptorPool, nullptr);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(vulkanDevice.getDevice(), frameUniformBuffers[i], nullptr);
        vkFreeMemory(vulkanDevice.getDevice(), frameUniformBufferMemories[i], nullptr);
    }

    vkDestroyDescriptorSetLayout(vulkanDevice.getDevice(), gBufferDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanDevice.getDevice(), frameDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(vulkanDevice.getDevice(), objectDescriptorSetLayout, nullptr);

    vkDestroyImageView(vulkanDevice.getDevice(), gBufferPositionView, nullptr);
    vkDestroyImage(vulkanDevice.getDevice(), gBufferPosition, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), gBufferPositionMemory, nullptr);

    vkDestroyImageView(vulkanDevice.getDevice(), gBufferNormalView, nullptr);
    vkDestroyImage(vulkanDevice.getDevice(), gBufferNormal, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), gBufferNormalMemory, nullptr);

    vkDestroyImageView(vulkanDevice.getDevice(), gBufferAlbedoView, nullptr);
    vkDestroyImage(vulkanDevice.getDevice(), gBufferAlbedo, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), gBufferAlbedoMemory, nullptr);

    vkDestroyImageView(vulkanDevice.getDevice(), depthImageView, nullptr);
    vkDestroyImage(vulkanDevice.getDevice(), depthImage, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), depthImageMemory, nullptr);

    for (int i = 0; i < 4; i++) {
        vkDestroyFramebuffer(vulkanDevice.getDevice(), cascadeFramebuffers[i], nullptr);
        vkDestroyImageView(vulkanDevice.getDevice(), cascadeImageViews[i], nullptr);
        vkDestroyImage(vulkanDevice.getDevice(), cascadeImages[i], nullptr);
        vkFreeMemory(vulkanDevice.getDevice(), cascadeImageMemories[i], nullptr);
    }

    vkDestroySampler(vulkanDevice.getDevice(), shadowMapSampler, nullptr);

    vkDestroyRenderPass(vulkanDevice.getDevice(), shadowRenderPass, nullptr);

    for (auto sem : imageAvailableSemaphores) {
        vkDestroySemaphore(vulkanDevice.getDevice(), sem, nullptr);
    }
    for (auto sem : renderFinishedSemaphores) {
        vkDestroySemaphore(vulkanDevice.getDevice(), sem, nullptr);
    }
    for (auto fence : inFlightFences) {
        vkDestroyFence(vulkanDevice.getDevice(), fence, nullptr);
    }

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(vulkanDevice.getDevice(), framebuffer, nullptr);
    }

    vkDestroyPipeline(vulkanDevice.getDevice(), geometryPipeline, nullptr);
    vkDestroyPipelineLayout(vulkanDevice.getDevice(), geometryPipelineLayout, nullptr);
    vkDestroyPipeline(vulkanDevice.getDevice(), lightingPipeline, nullptr);
    vkDestroyPipelineLayout(vulkanDevice.getDevice(), lightingPipelineLayout, nullptr);

    vkDestroyRenderPass(vulkanDevice.getDevice(), renderPass, nullptr);

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(vulkanDevice.getDevice(), imageView, nullptr);
    }

    vkDestroySwapchainKHR(vulkanDevice.getDevice(), swapChain, nullptr);

    vkFreeCommandBuffers(vulkanDevice.getDevice(), vulkanDevice.getCommandPool(),
        static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

    vulkanDevice.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
}

// --- Debug Callback ---
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "[Validation] " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

// --- Create Debug Utils Messenger EXT ---
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}