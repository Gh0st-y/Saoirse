#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <set>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanDevice {
public:
    void init(GLFWwindow* window);
    void cleanup();

    // Accessors
    VkDevice getDevice() const { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }
    VkSurfaceKHR getSurface() const { return surface; }
    VkCommandPool getCommandPool() const { return commandPool; }
    bool getEnableValidationLayers() const { return enableValidationLayers; }

    // Queue family / swap chain support — called by Renderer during swap chain setup
    QueueFamilyIndices findQueueFamilies() const;
    SwapChainSupportDetails querySwapChainSupport() const;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // Buffer / image helpers — used by ResourceManager and Renderer
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createImage(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory);
    void createImage(uint32_t width, uint32_t height, uint32_t arrayLayers,
        VkImageCreateFlags flags, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory);
    void transitionImageLayout(VkImage image, VkFormat format,
        VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image,
        uint32_t width, uint32_t height);

    // Single-time command helpers
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    bool isDeviceSuitable(VkPhysicalDevice dev);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev) const;

    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
};