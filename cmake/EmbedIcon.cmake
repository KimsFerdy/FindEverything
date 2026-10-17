# turns assets/findeverything.png into a byte array header so the binary carries its own window icon.
# (gcc 13 has no #embed yet)
set(FE_ICON_PNG ${CMAKE_SOURCE_DIR}/assets/findeverything.png)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FE_ICON_PNG})

file(READ ${FE_ICON_PNG} FE_ICON_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FE_ICON_BYTES "${FE_ICON_HEX}")
file(WRITE ${CMAKE_BINARY_DIR}/generated/AppIcon.h
    "#pragma once\n// generated from assets/findeverything.png, don't edit\n"
    "inline constexpr unsigned char kAppIconPng[] = {${FE_ICON_BYTES}};\n")
