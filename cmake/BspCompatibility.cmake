# Narrow, hash-checked source overrides; registry and SDK files stay intact.
idf_component_get_property(chronos_bsp_dir badgeteam__badge-bsp COMPONENT_DIR)
idf_component_get_property(chronos_mipi_dir nicolaielectronics__mipi_dsi_abstraction COMPONENT_DIR)
set(chronos_compat_dir "${CMAKE_BINARY_DIR}/chronos_bsp_compat")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_SOURCE_DIR}/tools/prepare_bsp_compat.py"
        --bsp-dir "${chronos_bsp_dir}" --mipi-dir "${chronos_mipi_dir}"
        --output "${chronos_compat_dir}"
    RESULT_VARIABLE chronos_compat_result
    ERROR_VARIABLE chronos_compat_error)
if(NOT chronos_compat_result EQUAL 0)
    message(FATAL_ERROR "CHRONOS BSP compatibility review required: ${chronos_compat_error}")
endif()

function(chronos_replace_source component original replacement)
    idf_component_get_property(component_lib ${component} COMPONENT_LIB)
    get_target_property(component_sources ${component_lib} SOURCES)
    list(FIND component_sources "${original}" source_index)
    if(source_index LESS 0)
        message(FATAL_ERROR "CHRONOS override could not find ${original} in ${component}")
    endif()
    list(REMOVE_AT component_sources ${source_index})
    list(APPEND component_sources "${replacement}")
    set_property(TARGET ${component_lib} PROPERTY SOURCES "${component_sources}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}")
endfunction()

chronos_replace_source(badgeteam__badge-bsp
    "${chronos_bsp_dir}/targets/tanmatsu/badge_bsp_display.c"
    "${chronos_compat_dir}/badge_bsp_display.c")
idf_component_get_property(chronos_bsp_lib badgeteam__badge-bsp COMPONENT_LIB)
target_include_directories(${chronos_bsp_lib} PRIVATE "${chronos_bsp_dir}/targets/tanmatsu")
chronos_replace_source(nicolaielectronics__mipi_dsi_abstraction
    "${chronos_mipi_dir}/dsi_panel_nicolaielectronics_st7701.c"
    "${chronos_compat_dir}/dsi_panel_nicolaielectronics_st7701.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/prepare_bsp_compat.py")
