project(visualization.spectrum)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(kodi REQUIRED)

if(WIN32)
  set(SOURCES src/directx_spectrum.cpp)
else(WIN32)
  # Locate XBMC bindings
  find_path(XBMC_INCLUDE_DIR xbmc/xbmc_addon_dll.h)

  find_package(OpenGLES)
  if(OPENGLES_FOUND)
    set(INCLUDES ${OPENGLES_INCLUDE_DIR})
    add_definitions(-DHAS_GLES)
    set(SPECTRUM_SOURCES src/opengl_spectrum.cpp
                         src/VisGUIShader.cpp
                         src/VisMatrixGLES.cpp
                         src/VisShader.cpp)
    set(DEPLIBS ${OPENGLES_LIBRARIES})
  else()
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    set(INCLUDES ${OpenGL_INCLUDE_DIR}
                 ${GLEW_INCLUDE_DIR})
    add_definitions(-DHAS_SDL_OPENGL)
    set(SPECTRUM_SOURCES src/opengl_spectrum.cpp)
    set(DEPLIBS ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES})
  endif()
endif()

include_directories(${INCLUDES}
                    ${KODI_INCLUDE_DIR})

build_addon(visualization.spectrum SPECTRUM DEPLIBS)

include(CPack)
