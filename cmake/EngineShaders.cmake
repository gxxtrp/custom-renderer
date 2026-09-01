# EngineShaders.cmake
# Standardized Slang-to-SPIR-V compilation helper

include(CMakeParseArguments)

function(engine_target_shaders TARGET_NAME)
    find_package(Slang REQUIRED)

    set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

    set(COMPILED_SPV_FILES)

    # Shaders are passed as tuples: SOURCE STAGE ENTRY OUTPUT_NAME
    # e.g.:
    # engine_target_shaders(my_target
    #     SHADERS
    #         "shaders/visibility/mesh_triangle.slang" task taskMain "mesh_triangle.task.spv"
    #         "shaders/visibility/mesh_triangle.slang" mesh meshMain "mesh_triangle.mesh.spv"
    #         "shaders/visibility/mesh_triangle.slang" fragment fragMain "mesh_triangle.frag.spv"
    # )
    set(oneValueArgs)
    set(multiValueArgs SHADERS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    list(LENGTH ARG_SHADERS SHADER_COUNT)
    math(EXPR REMAINDER "${SHADER_COUNT} % 4")
    if(NOT REMAINDER EQUAL 0)
        message(FATAL_ERROR "engine_target_shaders: SHADERS must be sets of 4 items: SOURCE STAGE ENTRY OUTPUT_NAME")
    endif()

    math(EXPR NUM_TUPLES "${SHADER_COUNT} / 4")
    if(NUM_TUPLES GREATER 0)
        math(EXPR LAST_INDEX "${NUM_TUPLES} - 1")
        foreach(INDEX RANGE 0 ${LAST_INDEX})
            math(EXPR BASE_IDX "${INDEX} * 4")
            list(GET ARG_SHADERS ${BASE_IDX} REL_SOURCE)
            math(EXPR STAGE_IDX "${BASE_IDX} + 1")
            list(GET ARG_SHADERS ${STAGE_IDX} STAGE)
            math(EXPR ENTRY_IDX "${BASE_IDX} + 2")
            list(GET ARG_SHADERS ${ENTRY_IDX} ENTRY)
            math(EXPR OUT_IDX "${BASE_IDX} + 3")
            list(GET ARG_SHADERS ${OUT_IDX} OUT_NAME)

            if(IS_ABSOLUTE "${REL_SOURCE}")
                set(SRC_PATH "${REL_SOURCE}")
            else()
                set(SRC_PATH "${CMAKE_SOURCE_DIR}/${REL_SOURCE}")
            endif()

            set(OUT_PATH "${SHADER_OUT_DIR}/${OUT_NAME}")

            add_custom_command(
                OUTPUT "${OUT_PATH}"
                COMMAND Slang::slangc "${SRC_PATH}" -target spirv -profile spirv_1_5 -stage ${STAGE} -entry ${ENTRY} -o "${OUT_PATH}"
                DEPENDS "${SRC_PATH}" Slang::slangc
                COMMENT "Compiling Slang [${STAGE}:${ENTRY}] ${REL_SOURCE} -> ${OUT_NAME}"
                VERBATIM
            )

            list(APPEND COMPILED_SPV_FILES "${OUT_PATH}")
        endforeach()
    endif()

    if(COMPILED_SPV_FILES)
        set(CUSTOM_TARGET_NAME "${TARGET_NAME}_shaders")
        add_custom_target(${CUSTOM_TARGET_NAME} DEPENDS ${COMPILED_SPV_FILES})
        add_dependencies(${TARGET_NAME} ${CUSTOM_TARGET_NAME})
        target_compile_definitions(${TARGET_NAME} PRIVATE ENGINE_SHADER_DIR="${SHADER_OUT_DIR}")
    endif()
endfunction()
