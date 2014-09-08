cmake_minimum_required(VERSION 2.6)
project (adlmidi)

# Options
option (BuildForAMD_X86_64 "Build for AMD x86_64 system" OFF)
option (BuildForCore2_X86_64 "Build for Intel Core2 x86_64 system" OFF)

# Audio backend
set (DefaultAudio jack CACHE STRING "Default audio driver - sdl or jack")

# compatibility
add_definitions(-Dstricmp=strcasecmp)
add_definitions(-Dstrnicmp=strncasecmp)

# Pkgconfig is required
find_package (PkgConfig REQUIRED)
if (PKG_CONFIG_FOUND)
    message(STATUS "Found pkg-config ${PKG_CONFIG_EXECUTABLE}")
else (PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config required but not found")
endif (PKG_CONFIG_FOUND)

# SDL is always necessary for now, even when JACK audio is used,
# as it is used for mutexes and threading.
find_package(SDL REQUIRED)
if (SDL_FOUND)
    message(STATUS "Found SDL ${SDL_VERSION}")
else (SDL_FOUND)
    message(FATAL_ERROR "SDL required but not found")
endif (SDL_FOUND)
include_directories(${SDL_INCLUDE_DIR})
set(AUDIO_LIBS ${AUDIO_LIBS} ${SDL_LIBRARY})

# Alsa
pkg_check_modules (ALSA REQUIRED alsa>=1.0.17)
if (ALSA_FOUND)
    message(STATUS "Found Alsa ${ALSA_VERSION}")
else (ALSA_FOUND)
    message(FATAL_ERROR "Alsa >=1.0.17 required but not found")
endif (ALSA_FOUND)
include_directories(${ALSA_INCLUDE_DIRS})
set(SEQ_LIBS ${SEQ_LIBS} ${ALSA_LIBRARIES})

# Jack
pkg_check_modules (JACK jack>=0.115.6)
if (JACK_FOUND)
    message(STATUS "Found jack ${JACK_VERSION}")
else (JACK_FOUND)
    message(STATUS "Jack >=0.115.6 required but not found, defaulting to SDL")
    set(DefaultAudio "sdl")
endif (JACK_FOUND)
include_directories(${JACK_INCLUDE_DIRS})
set(AUDIO_LIBS ${AUDIO_LIBS} ${JACK_LIBRARIES})

# Qt
# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
find_package(Qt5Widgets)
if (Qt5Widgets_FOUND)
    message(STATUS "Found Qt5Widgets ${Qt5Widgets_VERSION_STRING}")

    set(QT_ENABLED 1)
    add_definitions(${Qt5Widgets_DEFINITIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
    set(QT_LIBS Qt5::Widgets)
endif (Qt5Widgets_FOUND)

# Set definitions for audio backend
if (${DefaultAudio} STREQUAL "jack")
    message(STATUS "Audio driver is Jack")
    add_definitions(-DAUDIO_JACK)
elseif (${DefaultAudio} STREQUAL "sdl")
    message(STATUS "Audio driver is SDL")
    add_definitions(-DAUDIO_SDL)
else (${DefaultAudio} STREQUAL "sdl")
    message(FATAL_ERROR "Invalid DefaultAudio selection: " ${DefaultAudio})
endif (${DefaultAudio} STREQUAL "jack")

# Set a default build type for single-configuration
# CMake generators if no build type is set.
IF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE RelWithDebInfo)
ENDIF(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)

# Compiler flags
set (CMAKE_CXX_FLAGS "-std=c++11 -pedantic -Wall -Wextra" CACHE STRING "Common CXX compiler flags" FORCE)
set (CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -ggdb" CACHE STRING "Debugging CXX compiler flags" FORCE)
if (BuildForAMD_X86_64)
    set (OPTFLAGS "-O3 -march=athlon64 -m64 -Wall -ffast-math -fno-finite-math-only -fomit-frame-pointer")
else (BuildForAMD_X86_64)
    if (BuildForCore2_X86_64)
        set (OPTFLAGS "-O3 -march=core2 -m64 -Wall -ffast-math -fno-finite-math-only -fomit-frame-pointer")
    else (BuildForCore2_X86_64)
        set (OPTFLAGS "-O3 -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer")
    endif (BuildForCore2_X86_64)
endif (BuildForAMD_X86_64)
set (CMAKE_CXX_FLAGS_RELEASE "${OPTFLAGS}" CACHE STRING "Release CXX compiler flags" FORCE)
set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${OPTFLAGS} -g3 -ggdb" CACHE STRING "Release w/ debugging CXX compiler flags" FORCE)

# Target definitions
link_libraries(${LIBS})

set(adlmidi_HEADERS
    adldata.hh
    audioout.hh
    config.hh
    midievt.hh
    midi_symbols_256.hh
    parseargs.hh
    ui.hh
)
add_library(oplsynth STATIC
    oplsynth/OPL3.cpp
    oplsynth/dosbox_opl.cpp
    oplsynth/dosbox_dbopl.cpp
    oplsynth/ymf262.cpp
)

if (QT_ENABLED)
    # TODO: Qt should provide a new main object instead of
    # an ancillary one.
    # Launch synthesizer in a thread.
    set(adlmidi_QT qtconsole.cc)
endif (QT_ENABLED)

add_library(adlmidi_shared STATIC
    adldata.cc
    midievt.cc
    ui.cc
    audioout.cc
    parseargs.cc
    ${adlmidi_QT}
    ${adlmidi_HEADERS}
)
target_link_libraries(adlmidi_shared oplsynth ${AUDIO_LIBS} ${QT_LIBS})

add_executable(adlmidi midiplay.cc)
target_link_libraries(adlmidi adlmidi_shared) 

add_executable(adlseq seqplay.cc)
target_link_libraries(adlseq adlmidi_shared ${SEQ_LIBS}) 

add_executable(adljack jackplay.cc)
target_link_libraries(adljack adlmidi_shared)

add_executable(gen_adldata gen_adldata.cc)
target_link_libraries(gen_adldata oplsynth)
add_executable(dumpmiles dumpmiles.cc)
add_executable(dumpbank dumpbank.cc)

