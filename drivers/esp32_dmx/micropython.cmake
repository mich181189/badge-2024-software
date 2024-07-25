# Create an INTERFACE library for our C module.
add_library(usermod_esp32_dmx INTERFACE)

# Add our source files to the lib
target_sources(usermod_esp32_dmx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/mp_dmx.c
    ${CMAKE_CURRENT_LIST_DIR}/dmx.c
)

# Add the current directory as an include directory.
target_include_directories(usermod_esp32_dmx INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_esp32_dmx)
target_link_libraries(usermod_esp32_dmx INTERFACE ${MICROPY_TARGET})