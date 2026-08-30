add_library(tests-common INTERFACE)
target_include_directories(tests-common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(tests-common INTERFACE engine::core)
target_compile_features(tests-common INTERFACE cxx_std_23)

add_library(tests::common ALIAS tests-common)
