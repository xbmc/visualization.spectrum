PROJECT(visualization.spectrum)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

include(AddonVersion.cmake)
addon_version(visualization.spectrum SPECTRUM)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

OPTION(BUILD_DX    "Build DirectX version?" OFF)
OPTION(PACKAGE_ZIP "Package Zip file?" OFF)

IF(WIN32)
  SET(BINDING_FILE ${XBMC_BINDINGS}.zip)
  message (STATUS "downloading XBMC bindings: " ${BINDING_FILE})
  file(DOWNLOAD http://mirrors.xbmc.org/build-deps/win32/${BINDING_FILE} ${CMAKE_BINARY_DIR}/${BINDING_FILE} STATUS STATUSLIST SHOW_PROGRESS)
  LIST(GET STATUSLIST 0 VALUE)
  IF(${VALUE} STRGREATER "0")
    LIST(GET STATUSLIST 1 VALUE)
    message (STATUS "failed to download XBMC bindings: " ${VALUE})
  ENDIF(${VALUE} STRGREATER "0")
  execute_process(
   COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_BINARY_DIR}/${BINDING_FILE}
  )
  FIND_PATH(XBMC_INCLUDE_DIR xbmc/xbmc_addon_dll.h ${CMAKE_BINARY_DIR}/include)
  INCLUDE_DIRECTORIES(${XBMC_INCLUDE_DIR})
  ADD_DEFINITIONS(-DTARGET_WINDOWS)
  SET(SOURCES src/directx_spectrum.cpp)
ELSE(WIN32)
  # Locate XBMC bindings
  FIND_PATH(XBMC_INCLUDE_DIR xbmc/xbmc_addon_dll.h)

  FIND_PACKAGE(OpenGLES)
  IF(OPENGLES_FOUND)
    INCLUDE_DIRECTORIES(${OPENGLES_INCLUDE_DIR}
                        ${XBMC_INCLUDE_DIR})
    ADD_DEFINITIONS(-DHAS_GLES)
    SET(SOURCES src/opengl_spectrum.cpp
                src/VisGUIShader.cpp
                src/VisMatrixGLES.cpp
                src/VisShader.cpp)
    SET(OPENGL_LIBRARIES ${OPENGLES_LIBRARIES})
  ELSE(OPENGLES_FOUND)
    FIND_PACKAGE(OpenGL REQUIRED)
    FIND_PACKAGE(GLEW REQUIRED)
    INCLUDE_DIRECTORIES(${OpenGL_INCLUDE_DIR}
                        ${GLEW_INCLUDE_DIR}
                        ${XBMC_INCLUDE_DIR})
    ADD_DEFINITIONS(-DHAS_SDL_OPENGL)
    SET(SOURCES src/opengl_spectrum.cpp)
    SET(OPENGL_LIBRARIES ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES})
  ENDIF(OPENGLES_FOUND)
ENDIF(WIN32)

ADD_LIBRARY(spectrum ${SOURCES})
IF(NOT WIN32)
  TARGET_LINK_LIBRARIES(spectrum ${OPENGL_LIBRARIES})
ENDIF(NOT WIN32)
SET_TARGET_PROPERTIES(spectrum PROPERTIES VERSION ${SPECTRUM_VERSION}
                                          SOVERSION 13.0
                                          PREFIX "")

IF(PACKAGE_ZIP)
  # Pack files together to create an archive
  INSTALL(DIRECTORY visualization.spectrum DESTINATION ./)
  IF(WIN32)
    INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/spectrum.dll DESTINATION visualization.spectrum)
  ELSE(WIN32)
    INSTALL(TARGETS spectrum DESTINATION ./visualization.spectrum)
  ENDIF(WIN32)
  SET(CPACK_GENERATOR "ZIP")
  SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
  SET(CPACK_PACKAGE_FILE_NAME ${CMAKE_PROJECT_NAME}-${SPECTRUM_VERSION})
  INCLUDE(CPack)
ELSE(PACKAGE_ZIP)
  INSTALL(TARGETS spectrum DESTINATION lib/xbmc/addons/visualization.spectrum)
  INSTALL(DIRECTORY visualization.spectrum DESTINATION share/xbmc/addons)
ENDIF(PACKAGE_ZIP)
