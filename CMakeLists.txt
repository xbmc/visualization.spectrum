project(visualization.spectrum)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)

if(WIN32)
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
  find_package(OpenGL)
  if(OPENGL_FOUND)
    set(INCLUDES ${OPENGL_INCLUDE_DIR})
    add_definitions(-DHAS_OPENGL)
    set(SPECTRUM_SOURCES src/opengl_spectrum.cpp)
    set(DEPLIBS ${OPENGL_LIBRARIES})
  else()
    find_package(OpenGLES2)
    if(OPENGLES2_FOUND)
      set(INCLUDES ${OPENGLES2_INCLUDE_DIR})
      add_definitions(-DHAS_GLES2)
      set(SPECTRUM_SOURCES src/opengl_spectrum.cpp
                           src/VisGUIShader.cpp
                           src/VisMatrixGLES.cpp
                           src/VisShader.cpp)
      set(DEPLIBS ${OPENGLES2_LIBRARIES})
    else()
      message(FATAL_ERROR "No rendering API found. Bailing")
    endif()
  endif()
endif()

include_directories(${INCLUDES}
                    ${KODI_INCLUDE_DIR})

build_addon(visualization.spectrum SPECTRUM DEPLIBS)

include(CPack)
