#CXX=i686-w64-mingw32-g++ -static
CXX=g++
CXXLINK=$(CXX)

#DEBUG=-O0 -fno-inline -D_GLIBCXX_DEBUG -g -fstack-protector-all -fdata-sections

DEBUG=-Ofast -g

#DEBUG += -fno-tree-vectorize

# -march=pentium -mno-sse -mno-sse2 -mno-sse3 -mmmx

#CPPFLAGS+=-DAUDIO_SDL
CPPFLAGS+=$$(pkg-config --cflags sdl)
LDLIBS+=$$(pkg-config --libs sdl)
CPPFLAGS += $(SDL)

LDLIBS+=-lasound # for alsaseq
CPPFLAGS += -std=c++11 -pedantic -Wall -Wextra

# jack
CPPFLAGS+=-DAUDIO_JACK $$(pkg-config --cflags jack)
LDLIBS+=$$(pkg-config --libs jack)

# Official OPL sampling rate
#CPPFLAGS += -DPCM_RATE=49716
# More convenient frequency, especially for jack
CPPFLAGS += -DPCM_RATE=48000

include make.rules

