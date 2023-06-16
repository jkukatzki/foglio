# Install script for directory: C:/nap/compiled_0.6.0/apps/foglio/module

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/nap/compiled_0.6.0/apps/foglio/bin_package")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/FreeImage" TYPE FILE FILES
    "C:/nap/compiled_0.6.0/system_modules/naprender/thirdparty/FreeImage/msvc/x86_64/license-fi.txt"
    "C:/nap/compiled_0.6.0/system_modules/naprender/thirdparty/FreeImage/msvc/x86_64/license-gplv2.txt"
    "C:/nap/compiled_0.6.0/system_modules/naprender/thirdparty/FreeImage/msvc/x86_64/license-gplv3.txt"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/assimp" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/naprender/thirdparty/assimp/source/LICENSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/SDL2" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/naprender/thirdparty/SDL2/COPYING.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/Vulkan SDK" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/naprender/thirdparty/vulkansdk/LICENSE.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/glslang" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/naprender/thirdparty/glslang/source/LICENSE.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/SPIRV-Cross" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/naprender/thirdparty/SPIRV-Cross/source/LICENSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/tclap" TYPE FILE FILES "C:/nap/compiled_0.6.0/thirdparty/tclap/COPYING")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/system_modules/naprender" TYPE DIRECTORY FILES "C:/nap/compiled_0.6.0/system_modules/naprender/data")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/FFmpeg" TYPE FILE FILES
    "C:/nap/compiled_0.6.0/system_modules/napvideo/thirdparty/ffmpeg/licenses/COPYING.LGPLv2.1"
    "C:/nap/compiled_0.6.0/system_modules/napvideo/thirdparty/ffmpeg/licenses/COPYING.LGPLv3"
    "C:/nap/compiled_0.6.0/system_modules/napvideo/thirdparty/ffmpeg/licenses/LICENSE.md"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/FFmpeg" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/napvideo/thirdparty/ffmpeg/ffmpeg-3.4.2.tar.xz")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/FreeType" TYPE FILE FILES
    "C:/nap/compiled_0.6.0/system_modules/napfont/thirdparty/freetype/source/docs/FTL.TXT"
    "C:/nap/compiled_0.6.0/system_modules/napfont/thirdparty/freetype/source/docs/GPLv2.TXT"
    "C:/nap/compiled_0.6.0/system_modules/napfont/thirdparty/freetype/source/docs/LICENSE.TXT"
    "C:/nap/compiled_0.6.0/system_modules/napfont/thirdparty/freetype/source/docs/VERSIONS.TXT"
    "C:/nap/compiled_0.6.0/system_modules/napfont/thirdparty/freetype/source/docs/formats.txt"
    "C:/nap/compiled_0.6.0/system_modules/napfont/thirdparty/freetype/source/docs/raster.txt"
    "C:/nap/compiled_0.6.0/system_modules/napfont/thirdparty/freetype/source/README"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/GLM" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/napmath/thirdparty/glm/copying.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/mpg123" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/napaudio/thirdparty/mpg123/source/COPYING")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/libsndfile" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/napaudio/thirdparty/libsndfile/source/COPYING")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/licenses/PortAudio" TYPE FILE FILES "C:/nap/compiled_0.6.0/system_modules/napaudio/thirdparty/portaudio/msvc/x86_64/LICENSE.txt")
endif()
