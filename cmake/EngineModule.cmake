# EngineModule.cmake
# Standardized module creation macro/function

include(CMakeParseArguments)
include("${CMAKE_CURRENT_LIST_DIR}/CompilerWarnings.cmake")

function(engine_add_module)
    set(options)
    set(oneValueArgs NAME NAMESPACE)
    set(multiValueArgs PUBLIC_DEPS PRIVATE_DEPS PUBLIC_INCLUDES PRIVATE_INCLUDES SOURCES HEADERS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "engine_add_module: 'NAME' argument is required.")
    endif()

    if(NOT ARG_NAMESPACE)
        set(ARG_NAMESPACE "engine")
    endif()

    set(TARGET_NAME "${ARG_NAMESPACE}-${ARG_NAME}")
    set(ALIAS_NAME "${ARG_NAMESPACE}::${ARG_NAME}")

    # Gather headers and sources if not provided explicitly
    if(NOT ARG_HEADERS)
        file(GLOB_RECURSE ARG_HEADERS 
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        )
    endif()

    if(NOT ARG_SOURCES)
        file(GLOB_RECURSE ARG_SOURCES 
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        )
    endif()

    if(NOT ARG_SOURCES AND NOT ARG_HEADERS)
        message(FATAL_ERROR "engine_add_module: No source or header files found for module '${TARGET_NAME}'.")
    endif()

    # If only headers exist, create an INTERFACE library, else STATIC
    if(ARG_SOURCES)
        add_library(${TARGET_NAME} STATIC ${ARG_SOURCES} ${ARG_HEADERS})
        target_include_directories(${TARGET_NAME}
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        if(ARG_PUBLIC_INCLUDES)
            target_include_directories(${TARGET_NAME} PUBLIC ${ARG_PUBLIC_INCLUDES})
        endif()
        if(ARG_PRIVATE_INCLUDES)
            target_include_directories(${TARGET_NAME} PRIVATE ${ARG_PRIVATE_INCLUDES})
        endif()
        if(ARG_PUBLIC_DEPS)
            target_link_libraries(${TARGET_NAME} PUBLIC ${ARG_PUBLIC_DEPS})
        endif()
        if(ARG_PRIVATE_DEPS)
            target_link_libraries(${TARGET_NAME} PRIVATE ${ARG_PRIVATE_DEPS})
        endif()
        set_target_properties(${TARGET_NAME} PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )
        engine_set_compiler_warnings(${TARGET_NAME})
    else()
        add_library(${TARGET_NAME} INTERFACE ${ARG_HEADERS})
        target_include_directories(${TARGET_NAME}
            INTERFACE
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
        )
        if(ARG_PUBLIC_INCLUDES)
            target_include_directories(${TARGET_NAME} INTERFACE ${ARG_PUBLIC_INCLUDES})
        endif()
        if(ARG_PUBLIC_DEPS)
            target_link_libraries(${TARGET_NAME} INTERFACE ${ARG_PUBLIC_DEPS})
        endif()
        target_compile_features(${TARGET_NAME} INTERFACE cxx_std_23)
    endif()

    add_library(${ALIAS_NAME} ALIAS ${TARGET_NAME})
endfunction()
