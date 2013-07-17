PROJECT(visualization.spectrum)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

FIND_PACKAGE(xbmc REQUIRED)

addon_version(visualization.spectrum SPECTRUM)

IF(WIN32)
  SET(SOURCES src/directx_spectrum.cpp)
ELSE(WIN32)
  # Locate XBMC bindings
  FIND_PATH(XBMC_INCLUDE_DIR xbmc/xbmc_addon_dll.h)

  FIND_PACKAGE(OpenGLES)
  IF(OPENGLES_FOUND)
    SET(INCLUDES ${OPENGLES_INCLUDE_DIR})
    ADD_DEFINITIONS(-DHAS_GLES)
    SET(SOURCES src/opengl_spectrum.cpp
                src/VisGUIShader.cpp
                src/VisMatrixGLES.cpp
                src/VisShader.cpp)
    SET(DEPLIBS ${OPENGLES_LIBRARIES})
  ELSE(OPENGLES_FOUND)
    FIND_PACKAGE(OpenGL REQUIRED)
    FIND_PACKAGE(GLEW REQUIRED)
    SET(INCLUDES ${OpenGL_INCLUDE_DIR}
                 ${GLEW_INCLUDE_DIR})
    ADD_DEFINITIONS(-DHAS_SDL_OPENGL)
    SET(SOURCES src/opengl_spectrum.cpp)
    SET(DEPLIBS ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES})
  ENDIF(OPENGLES_FOUND)
ENDIF(WIN32)

INCLUDE_DIRECTORIES(${INCLUDES}
                    ${XBMC_INCLUDE_DIR})

build_addon(visualization.spectrum SOURCES DEPLIBS SPECTRUM_VERSION)
install_addon(visualization.spectrum ${SPECTRUM_VERSION})
