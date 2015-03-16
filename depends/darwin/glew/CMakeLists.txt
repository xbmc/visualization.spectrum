project(glew)

cmake_minimum_required(VERSION 2.6)
enable_language(CXX)

include(ExternalProject)

externalproject_add(glew
                    SOURCE_DIR ${CMAKE_SOURCE_DIR}
                    UPDATE_COMMAND ""
                    CONFIGURE_COMMAND ""
                    BUILD_COMMAND SYSTEM=darwin GLEW_DEST=${CMAKE_INSTALL_PREFIX} make
                    INSTALL_COMMAND ""
                    BUILD_IN_SOURCE 1)

install(CODE "execute_process(COMMAND SYSTEM=darwin GLEW_DEST=${CMAKE_INSTALL_PREFIX} make install WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})")
