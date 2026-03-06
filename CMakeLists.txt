idf_component_register(
        SRCS "src/hid_gamepad.c"
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "src"
        PRIV_REQUIRES espressif__tinyusb usb
)

# tusb_config.h lives in src/ but must be visible to the espressif__tinyusb
# component (which doesn't depend on us). Add it to the global include path.
idf_build_set_property(INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/src" APPEND)

target_include_directories(${COMPONENT_LIB} PRIVATE
        "${IDF_PATH}/components/usb/include"
)