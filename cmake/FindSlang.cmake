# FindSlang.cmake
# Locates the Slang shader compiler (slangc)

find_program(SLANGC_EXECUTABLE
    NAMES slangc slangc.exe
    HINTS
        ENV VULKAN_SDK
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
    PATHS
        "C:/VulkanSDK/*/Bin"
        "/usr/local/bin"
        "/usr/bin"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Slang
    REQUIRED_VARS SLANGC_EXECUTABLE
)

if(Slang_FOUND AND NOT TARGET Slang::slangc)
    add_executable(Slang::slangc IMPORTED GLOBAL)
    set_target_properties(Slang::slangc PROPERTIES
        IMPORTED_LOCATION "${SLANGC_EXECUTABLE}"
    )
endif()
