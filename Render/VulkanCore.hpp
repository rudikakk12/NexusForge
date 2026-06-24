//
// Fájl: Render/VulkanCore.hpp
// Készítette: NexusForge Engine (Rick & Gem)
// Cél: Vulkan Ablakkezelés és Hardver Inicializálás
//
#pragma once

// 1. Vulkan fejlécek
#pragma once

// A sorrend életbevágó:
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
// ... a többi kódod ...
#include <stdexcept>
#include <vector>
#include <cstdlib>

namespace NF::Render {

    class VulkanCore {
    private:
        GLFWwindow* window;
        VkInstance instance;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device;
        VkQueue graphicsQueue;

        const uint32_t WIDTH = 1280;
        const uint32_t HEIGHT = 720;

        void initWindow() {
            glfwInit();
            // A GLFW-nek megmondjuk, hogy NE hozzon létre OpenGL kontextust, mert Vulkant használunk!
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Később dinamikus lesz

            window = glfwCreateWindow(WIDTH, HEIGHT, "NexusForge Titan Engine - Vulkan", nullptr, nullptr);
        }

        void initVulkan() {
            createInstance();
            pickPhysicalDevice();
            createLogicalDevice();
            std::cout << "[Vulkan] Hardver inicializalva!" << std::endl;
        }

        void createInstance() {
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "NexusForge";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "Titan Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_2;

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;

            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            createInfo.enabledExtensionCount = glfwExtensionCount;
            createInfo.ppEnabledExtensionNames = glfwExtensions;
            createInfo.enabledLayerCount = 0; // Később ide jöhet a Validation Layer a hibakereséshez

            if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Hiba: Nem sikerult letrehozni a Vulkan Instance-t!");
            }
        }

        void pickPhysicalDevice() {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
            if (deviceCount == 0) throw std::runtime_error("[Vulkan] Hiba: Nincs Vulkan-kompatibilis GPU!");

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            // Egyszerű logika: Kiválasztjuk az első elérhető GPU-t (Később lehet dedikált GPU-t keresni)
            physicalDevice = devices[0];
        }

        void createLogicalDevice() {
            // Egyelőre egyetlen Queue-t (Grafikus parancsokhoz) kérünk
            float queuePriority = 1.0f;
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = 0; // Feltételezzük, hogy a 0. család tud rajzolni
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            VkPhysicalDeviceFeatures deviceFeatures{};

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pQueueCreateInfos = &queueCreateInfo;
            createInfo.queueCreateInfoCount = 1;
            createInfo.pEnabledFeatures = &deviceFeatures;
            createInfo.enabledExtensionCount = 0;

            if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Hiba: Nem sikerult letrehozni a Logical Device-t!");
            }

            vkGetDeviceQueue(device, 0, 0, &graphicsQueue);
        }

        void mainLoop() {
            std::cout << "[Engine] Belepes a fohurokba..." << std::endl;
            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
                // ==========================================
                // IDE JÖN MAJD A PHASE 1 - PHASE 5 (Titan Pipeline hívás)
                // IDE JÖN A VULKAN RENDER (vkQueueSubmit)
                // ==========================================
            }
        }

        void cleanup() {
            vkDestroyDevice(device, nullptr);
            vkDestroyInstance(instance, nullptr);
            glfwDestroyWindow(window);
            glfwTerminate();
            std::cout << "[Engine] Tiszta leallas." << std::endl;
        }

    public:
        void run() {
            initWindow();
            initVulkan();
            mainLoop();
            cleanup();
        }
    };
}