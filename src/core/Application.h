#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "VulkanDevice.h"
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <chrono>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../renderer/Vertex.h"
#include "../renderer/Camera.h"
#include "../scene/GameObject.h"
#include "../scene/Scene.h"
#include "../renderer/Frustum.h"
#include <memory>

struct Cascade {
    glm::mat4 viewProj;
    float splitDepth;
};

class Application {
public:
    void run();

private:
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    void createSwapChain();
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void createImageViews();
    void createRenderPass();
    void createGeometryPipeline();
    void createLightingPipeline();
    void createGBufferDescriptorSetLayout();
    void createGBufferDescriptorSet();

    void createShadowRenderPass();
    void createShadowResources();   // creates the depth image/view + framebuffer for the shadow map
    void createShadowPipeline();

    void updateShadowMatrix();
    void updateCascades();
    std::array<Cascade, 4> cascades;
    std::array<float, 4> computeCascadeSplits(float nearClip, float farClip);
    std::array<glm::vec3, 8> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);

    void recordShadowPass(VkCommandBuffer cmdBuffer, uint32_t frameIndex);

    void createSpotShadowResources();
    void updateSpotShadowMatrices();
    void recordSpotShadowPass(VkCommandBuffer cmdBuffer, uint32_t frameIndex);

    void createPointShadowPipeline();

    void createPointShadowResources();
    void updatePointShadowMatrices();
    void recordPointShadowPass(VkCommandBuffer cmdBuffer, uint32_t frameIndex);

    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createFramebuffers();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t frameIndex);

    Mesh* loadMesh(const std::string& path);
    Texture* loadTexture(const std::string& path);

    void createDefaultNormalMap();

    void createMeshBuffers(Mesh* mesh);
    void destroyMesh(Mesh* mesh);
    void destroyTexture(Texture* texture);

    void createFrameDescriptorSetLayout();
    void createObjectDescriptorSetLayout();
    void createDescriptorPool();

    void createFrameUniformBuffer();
    void updateFrameUniformBuffer(uint32_t frameIndex);
    void createFrameDescriptorSet();

    void createGameObjectResources(GameObject& obj);   // creates UBO + descriptor set for one object
    void updateGameObjectUniformBuffer(GameObject& obj, uint32_t frameIndex);

    void createDepthResources();
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);

    void processInput(float deltaTime);
    void processMouseInput(double xpos, double ypos);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

    void createGBufferResources();

    void createSyncObjects();
    void drawFrame();

private:
    GLFWwindow* window = nullptr;
    VulkanDevice vulkanDevice;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Texture>> textures;
    Scene scene;

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    VkImage gBufferPosition = VK_NULL_HANDLE;
    VkDeviceMemory gBufferPositionMemory = VK_NULL_HANDLE;
    VkImageView gBufferPositionView = VK_NULL_HANDLE;

    VkImage gBufferNormal = VK_NULL_HANDLE;
    VkDeviceMemory gBufferNormalMemory = VK_NULL_HANDLE;
    VkImageView gBufferNormalView = VK_NULL_HANDLE;

    VkImage gBufferAlbedo = VK_NULL_HANDLE;
    VkDeviceMemory gBufferAlbedoMemory = VK_NULL_HANDLE;
    VkImageView gBufferAlbedoView = VK_NULL_HANDLE;

    VkRenderPass renderPass = VK_NULL_HANDLE;

    VkPipeline geometryPipeline = VK_NULL_HANDLE;
    VkPipelineLayout geometryPipelineLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout gBufferDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet gBufferDescriptorSet = VK_NULL_HANDLE;

    VkPipeline lightingPipeline = VK_NULL_HANDLE;
    VkPipelineLayout lightingPipelineLayout = VK_NULL_HANDLE;

    std::array<VkImage, MAX_SPOT_LIGHTS> spotShadowImages{};
    std::array<VkDeviceMemory, MAX_SPOT_LIGHTS> spotShadowMemories{};
    std::array<VkImageView, MAX_SPOT_LIGHTS> spotShadowViews{};
    std::array<VkFramebuffer, MAX_SPOT_LIGHTS> spotShadowFramebuffers{};
    VkSampler spotShadowSampler = VK_NULL_HANDLE;
    std::array<glm::mat4, MAX_SPOT_LIGHTS> spotLightMatrices{};

    std::array<VkImage, MAX_POINT_LIGHTS> pointShadowCubemaps{};
    std::array<VkDeviceMemory, MAX_POINT_LIGHTS> pointShadowMemories{};
    std::array<VkImageView, MAX_POINT_LIGHTS> pointShadowCubemapViews{};         // full cubemap view for sampling
    std::array<std::array<VkImageView, 6>, MAX_POINT_LIGHTS> pointShadowFaceViews{};  // per-face views for rendering
    std::array<std::array<VkFramebuffer, 6>, MAX_POINT_LIGHTS> pointShadowFramebuffers{};
    VkSampler pointShadowSampler = VK_NULL_HANDLE;

    VkPipeline pointShadowPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pointShadowPipelineLayout = VK_NULL_HANDLE;

    std::array<std::array<glm::mat4, 6>, MAX_POINT_LIGHTS> pointLightMatrices{};
    VkRenderPass pointShadowRenderPass = VK_NULL_HANDLE;

    VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
    VkPipeline shadowPipeline = VK_NULL_HANDLE;
    VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;

    std::array<VkImage, 4> cascadeImages{};
    std::array<VkDeviceMemory, 4> cascadeImageMemories{};
    std::array<VkImageView, 4> cascadeImageViews{};
    std::array<VkFramebuffer, 4> cascadeFramebuffers{};
    VkSampler shadowMapSampler = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> swapChainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    VkDescriptorSetLayout frameDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout objectDescriptorSetLayout = VK_NULL_HANDLE;

    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> frameUniformBuffers{};
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> frameUniformBufferMemories{};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> frameUniformBuffersMapped{};
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> frameDescriptorSets{};

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    Camera camera;
    Frustum currentFrustum;
    float lastFrameTime = 0.0f;
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    bool firstMouse = true;
    bool invertMouseX = false;
    bool invertMouseY = false;

    const uint32_t SPOT_SHADOW_SIZE = 1024;
    const uint32_t POINT_SHADOW_SIZE = 512;

    glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);
    const std::array<uint32_t, 4> CASCADE_SIZES = { 4096, 2048, 2048, 1024 };

    Texture* defaultNormalMap = nullptr;

    const uint32_t WIDTH = 1024;
    const uint32_t HEIGHT = 720;
};