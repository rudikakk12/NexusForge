cmake_minimum_required(VERSION 3.20)
project(NexusForge)

set(CMAKE_CXX_STANDARD 23)

# A D:\VULKAN mappád gyökere, ahol az Include és Lib vannak
set(VULKAN_SDK_PATH "D:/VULKAN")
set(GLFW_DIR "C:/Users/Rick/libs/glfw")
add_compile_options(-mavx2)
add_executable(NexusForge main.cpp
        Render/VulkanPipeline.hpp
        Render/VulkanCore.hpp
        Render/BasicMesher.hpp
        Core/MutationPipeline.hpp
        Core/MacroChunk.hpp
        Core/BlockPlacement.hpp
        Render/VulkanSwapchain.hpp
        Render/VulkanRenderer.hpp
        Render/VulkanCommandManager.hpp)

# Így a fordító megtalálja a D:/VULKAN/Include/vulkan/vulkan.h fájlt
target_include_directories(NexusForge PUBLIC
        "${VULKAN_SDK_PATH}/Include"
        "${GLFW_DIR}/include"
)

target_link_directories(NexusForge PRIVATE
        "${VULKAN_SDK_PATH}/Lib"
        "${GLFW_DIR}/lib-mingw-w64"
)

# Linkelés
target_link_libraries(NexusForge PRIVATE vulkan-1 glfw3)