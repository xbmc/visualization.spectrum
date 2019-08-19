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
else(WIN32)
  if(APP_RENDER_SYSTEM STREQUAL "gl" OR NOT APP_RENDER_SYSTEM)
    find_package(OpenGL)
  endif()
  if(OPENGL_FOUND)
    set(APP_RENDER_SYSTEM gl)
    set(INCLUDES ${OPENGL_INCLUDE_DIR})
    add_definitions(-DHAS_OPENGL)
    set(SPECTRUM_SOURCES src/opengl_spectrum.cpp
                         src/shaders/GUIShader.cpp
                         src/shaders/Matrix.cpp
                         src/shaders/Shader.cpp)
    set(SPECTRUM_HEADERS src/opengl_spectrum.h
                         src/shaders/GUIShader.h
                         src/shaders/Matrix.h
                         src/shaders/Shader.h)
    set(DEPLIBS ${OPENGL_LIBRARIES})
  else()
    find_package(OpenGLES2)
    if(OPENGLES2_FOUND)
      set(APP_RENDER_SYSTEM gles)
      set(INCLUDES ${OPENGLES2_INCLUDE_DIR})
      add_definitions(-DHAS_GLES2)
      set(SPECTRUM_SOURCES src/opengl_spectrum.cpp
                           src/shaders/GUIShader.cpp
                           src/shaders/Matrix.cpp
                           src/shaders/Shader.cpp)

      set(SPECTRUM_HEADERS src/opengl_spectrum.h
                           src/shaders/GUIShader.h
                           src/shaders/Matrix.h
                           src/shaders/Shader.h)
      set(DEPLIBS ${OPENGLES2_LIBRARIES})
    else()
      message(FATAL_ERROR "No rendering API found. Bailing")
    endif()
  endif()
endif()

message(STATUS "Configured render system: ${APP_RENDER_SYSTEM}")

include_directories(${INCLUDES}
                    ${KODI_INCLUDE_DIR}/..) # Hack way with "/..", need bigger Kodi cmake rework to match right include ways (becomes done in future)

build_addon(visualization.spectrum SPECTRUM DEPLIBS)

include(CPack)
