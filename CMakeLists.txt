cmake_minimum_required(VERSION 3.5)
project(visualization.spectrum)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)

if(WIN32)
  set(APP_RENDER_SYSTEM dx11)
  add_definitions(-DDIRECTX_SPEKTRUM_EXPORTS -D_WIN32PC -D_USRDLL)
  set(SHADER_FILES src/DefaultVertexShader.hlsl
                   src/DefaultPixelShader.hlsl)
  set(SHADER_INCLUDES)
  foreach( SHADER_FILE ${SHADER_FILES} )
    get_filename_component( FileName ${SHADER_FILE} NAME_WE )
    if(${FileName} MATCHES "(.+Vertex.+)")
      set(ShaderType vs)
    elseif(${FileName} MATCHES "(.+Pixel.+)")
      set(ShaderType ps)
    else()
      message(FATAL_ERROR ${SHADER_FILE}": Unknown shader type.")
    endif()
    set( IncFile src/${FileName}.inc )
    list( APPEND SHADER_INCLUDES ${IncFile} )

    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${IncFile}
      COMMAND fxc /nologo /T${ShaderType}_4_0_level_9_1 /Vn ${FileName}Code /Fh ${IncFile} ${SHADER_FILE}
      MAIN_DEPENDENCY ${SHADER_FILE}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      VERBATIM)
  endforeach(SHADER_FILE)
  add_custom_target(generate ALL DEPENDS ${SHADER_INCLUDES})
  set(SPECTRUM_SOURCES src/directx_spectrum.cpp)
else()
  find_package(glm REQUIRED)

  if(APP_RENDER_SYSTEM STREQUAL "gl" OR NOT APP_RENDER_SYSTEM)
    find_package(OpenGl REQUIRED)
    set(DEPLIBS ${OPENGL_LIBRARIES})
    set(INCLUDES ${OPENGL_INCLUDE_DIR})
    add_definitions(${OPENGL_DEFINITIONS})
  elseif(APP_RENDER_SYSTEM STREQUAL "gles")
    find_package(OpenGLES REQUIRED)
    set(DEPLIBS ${OPENGLES_LIBRARIES})
    set(INCLUDES ${OPENGLES_INCLUDE_DIR})
    add_definitions(${OPENGLES_DEFINITIONS})
  endif()

  set(SPECTRUM_SOURCES src/opengl_spectrum.cpp)

  include_directories(${GLM_INCLUDE_DIR})
endif()

message(STATUS "Configured render system: ${APP_RENDER_SYSTEM}")

include_directories(${INCLUDES}
                    ${KODI_INCLUDE_DIR}/..) # Hack way with "/..", need bigger Kodi cmake rework to match right include ways (becomes done in future)

build_addon(visualization.spectrum SPECTRUM DEPLIBS)

include(CPack)
