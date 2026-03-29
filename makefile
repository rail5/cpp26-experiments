LOCAL_GCC16_DIRECTORY ?= $(shell echo $$HOME)/assembly/not-mine/gcc

GCC_BUILD            := $(LOCAL_GCC16_DIRECTORY)/build
GCC_BINDIR           := $(GCC_BUILD)/gcc/
CXX                  := $(GCC_BINDIR)/xg++
TARGET               := $(shell $(CXX) -dumpmachine)

LIBSTDCXX_BUILD      := $(GCC_BUILD)/$(TARGET)/libstdc++-v3
LIBSTDCXX_INC        := $(LIBSTDCXX_BUILD)/include
LIBSTDCXX_INC_TARGET := $(LIBSTDCXX_INC)/$(TARGET)
LIBSTDCXX_LIB        := $(LIBSTDCXX_BUILD)/src/.libs

CXXFLAGS             := -std=c++26 -freflection -O2 -B$(GCC_BINDIR) -isystem $(LIBSTDCXX_INC) -isystem $(LIBSTDCXX_INC_TARGET) -isystem $(LOCAL_GCC16_DIRECTORY)/libstdc++-v3/libsupc++/
LDFLAGS              := -B$(GCC_BINDIR) -L$(LIBSTDCXX_LIB) -Wl,-rpath,$(LIBSTDCXX_LIB)


CPP_FILES := $(wildcard *.cpp)
BINARIES  := $(patsubst %.cpp,bin/%,$(CPP_FILES))

all: $(BINARIES)

bin/%: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f bin/*
