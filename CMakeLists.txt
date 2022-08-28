project( prodatum )
cmake_minimum_required( VERSION 3.21 )

if( NOT CMAKE_BUILD_TYPE )
  SET( CMAKE_BUILD_TYPE Release )
endif()

macro( SET_OPTION option value )
  set( ${option} ${value} CACHE INTERNAL "" FORCE )
endmacro()

SET_OPTION( BUILD_SHARED_LIBS OFF )
SET_OPTION( OPTION_BUILD_SHARED_LIBS OFF )
SET_OPTION( FLTK_BUILD_TEST OFF )
SET_OPTION( FLTK_BUILD_EXAMPLES OFF )

set( SYSEX_MAX_SIZE 1024 )
set( RINGBUFFER_WRITE 2048 )
set( RINGBUFFER_READ 2048 )
set( PATH_MAX 1024 )
set( LOG_BUFFER_SIZE 1048576 )
set( RES_FILES "" )

if( WIN32 )
  set( FLUID_EXECUTABLE "${CMAKE_BINARY_DIR}/lib/fltk/bin/${CMAKE_BUILD_TYPE}/fluid.exe" )
else()
  set( FLUID_EXECUTABLE "${CMAKE_BINARY_DIR}/lib/fltk/bin/fluid" )
endif( WIN32 )

set( UI_AUTOGEN "${CMAKE_BINARY_DIR}/ui.c" )

CONFIGURE_FILE( ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_SOURCE_DIR}/config.h )

option( BUILD_LIBS "Enable building libs" ON )

if( BUILD_LIBS )
  add_subdirectory( ${CMAKE_SOURCE_DIR}/lib/portmidi )
  add_subdirectory( ${CMAKE_SOURCE_DIR}/lib/fltk )
endif( BUILD_LIBS )

add_custom_command(
  OUTPUT ${UI_AUTOGEN}
  PRE_BUILD
  COMMAND "${FLUID_EXECUTABLE}"
          "-c"
          "${CMAKE_SOURCE_DIR}/prodatum.fl"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS fluid
)

include_directories ( 
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include 
  ${CMAKE_SOURCE_DIR}/lib/portmidi/pm_common
  ${CMAKE_SOURCE_DIR}/lib/portmidi/porttime
  ${CMAKE_SOURCE_DIR}/lib/fltk
  ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/include 
  ${CMAKE_BINARY_DIR}/lib/fltk
)

link_directories ( 
  ${CMAKE_SOURCE_DIR}/lib 
  ${CMAKE_BINARY_DIR}/lib/fltk/lib
  ${CMAKE_BINARY_DIR}/lib/fltk/lib/${CMAKE_BUILD_TYPE} 
  ${CMAKE_BINARY_DIR}/lib/portmidi
  ${CMAKE_BINARY_DIR}/lib/portmidi/${CMAKE_BUILD_TYPE} 
)

if ( WIN32 )
  set( RES_FILES "windows.rc" )
endif ( WIN32 )

if ( MINGW )
  set( CMAKE_RC_COMPILER_INIT windres )
  ENABLE_LANGUAGE(RC)
  SET( CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>" )
endif( MINGW )

set ( SOURCES
      src/cfg.cpp
      src/data.cpp
      src/debug.cpp
      src/Fl_Scope.cpp
      src/midi.cpp
      src/prodatum.cpp
      src/pxk.cpp
      src/ringbuffer.cpp
      src/widgets.cpp
)

add_executable ( prodatum ${SOURCES} ${UI_AUTOGEN} ${RES_FILES} )

# Build for Windows
if( WIN32 )
  # using gcc
  if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
    ADD_DEFINITIONS( -DWIN32 -DSYNCLOG -Wall -W -Wno-char-subscripts -Wno-write-strings -Wno-format )
    set( CMAKE_EXE_LINKER_FLAGS "-mwindows -static-libgcc -static-libstdc++" )
    set( ADDITIONAL_LIBRARIES winmm ole32 uuid comctl32 wsock32 )

  # using visual studio
  elseif( MSVC )
    ADD_DEFINITIONS( -DWIN32 -DSYNCLOG -D_CRT_SECURE_NO_WARNINGS )
    set( ADDITIONAL_LIBRARIES winmm ole32 uuid comctl32 wsock32 gdi32 )

    target_link_options(prodatum PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
  endif()
endif( WIN32 )

# Build for MacOS
if( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )
  ADD_DEFINITIONS( -std=c++14 -DUSE_MLOCK -DOSX -DSYNCLOG -Wall -W -Wno-char-subscripts -Wno-write-strings )
  set( ADDITIONAL_LIBRARIES "-framework Carbon -framework Cocoa -framework ApplicationServices -framework CoreMIDI -framework CoreAudio" )
endif( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )

# Build for Linux
if( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" )
	ADD_DEFINITIONS( -std=c++14 -DUSE_MLOCK -D__linux -DSYNCLOG -Wall -W -Wno-char-subscripts -Wno-write-strings )
	set( CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++" )
  set( ADDITIONAL_LIBRARIES X11 Xinerama Xft fontconfig asound pthread )
endif( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" )

target_include_directories( prodatum 
                            PRIVATE 
                            ${CMAKE_BUILD_DIR}/lib/fltk/lib/${CMAKE_BUILD_TYPE}
                            ${CMAKE_BUILD_DIR}/lib/portmidi/${CMAKE_BUILD_TYPE}
                            )

target_link_libraries ( prodatum fltk portmidi ${ADDITIONAL_LIBRARIES} )
