# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_DISPLAY)

if(CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
    zephyr_library_named(nice_oled_custom)
    zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/widgets/)
    zephyr_library_sources(custom_status_screen.c)
    zephyr_library_sources(widgets/util.c)
    zephyr_library_sources(widgets/bolt.c)

    if((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        zephyr_library_sources(widgets/status.c)
    else()
        zephyr_library_sources(widgets/art.c)
        zephyr_library_sources(widgets/peripheral_status.c)
    endif()
endif()

endif()
