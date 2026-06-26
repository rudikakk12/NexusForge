cmake_minimum_required(VERSION 3.20)
project(NexusForge)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# 1. RICK LOKÁLIS KÖRNYEZETI VÁLTOZÓI
# Szigorúan hardkódolt útvonalak a MinGW-w64 toolchain-hez és az SDK-khoz
# ==============================================================================
set(VULKAN_SDK_PATH "D:/VULKAN")
set(GLFW_DIR "C:/Users/Rick/libs/glfw")

# ==============================================================================
# 2. KRITIKUS FORDÍTÁSI OPCIÓK (TELJESÍTMÉNY)
# -mavx2: Nélkülözhetetlen a Data-Oriented Design (DOD) struktúra és
# a 256-bites SIMD memóriaműveletek futtatásához a Worker szálakon.
# ==============================================================================
add_compile_options(-mavx2)

# ==============================================================================
# 3. IMGUI FÜGGŐSÉG BEHÚZÁSA (DOCKING BRANCH)
# Mivel csak a core fájlok kellenek, futásidőben letöltjük a forrást,
# és magunk fordítjuk le a motorral együtt. Zéró precompiled bináris szívás.
# ==============================================================================
include(FetchContent)
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# Az ImGui és a GLFW+Vulkan backend forrásfájljainak összegyűjtése
set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        Core/Physics.hpp
        Core/Physics.hpp
        Render/VulkanMegaArena.hpp
        libs/stb_image.h
        libs/stb_image.h
)

# ==============================================================================
# 4. A NEXUSFORGE EXECUTABLE ÉS FORRÁSOK
# A CLion indexelés miatt a headereket is beletesszük a targetbe.
# ==============================================================================
add_executable(NexusForge
        main.cpp
        Render/VulkanPipeline.hpp
        Render/VulkanCore.hpp
        Render/BasicMesher.hpp
        Core/MutationPipeline.hpp
        Core/MacroChunk.hpp
        Core/BlockPlacement.hpp
        Render/VulkanSwapchain.hpp
        Render/VulkanRenderer.hpp
        Render/VulkanCommandManager.hpp
        ${IMGUI_SOURCES}
)

# ==============================================================================
# 5. INCLUDE KÖNYVTÁRAK
# ==============================================================================
# A motor natív függőségei (Vulkan & GLFW)
target_include_directories(NexusForge PUBLIC
        "${VULKAN_SDK_PATH}/Include"
        "${GLFW_DIR}/include"
)
# Az ImGui includjai, beleértve a backend mappát a vulkan/glfw implementációkhoz
target_include_directories(NexusForge PRIVATE
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

# ==============================================================================
# 6. LINKELÉSI KÖNYVTÁRAK
# Szigorúan a te lokális MinGW-w64 könyvtáraidhoz igazítva.
# ==============================================================================
target_link_directories(NexusForge PRIVATE
        "${VULKAN_SDK_PATH}/Lib"
        "${GLFW_DIR}/lib-mingw-w64"
)

# ==============================================================================
# 7. KÖNYVTÁRAK ÖSSZEKÖTÉSE (LINKING)
# ==============================================================================
target_link_libraries(NexusForge PRIVATE vulkan-1 glfw3)