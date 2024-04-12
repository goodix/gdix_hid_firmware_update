CXX ?= g++
#CXX := /home/public/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++

CXXFLAGS += -Wall -O3 -fno-strict-aliasing
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE

UPDATESRC := $(wildcard *.cpp) \
			 $(wildcard gt7868q/*.cpp) \
			 $(wildcard gtx2/*.cpp) \
			 $(wildcard gtx3/*.cpp) \
			 $(wildcard gtx5/*.cpp) \
			 $(wildcard gtx8/*.cpp) \
			 $(wildcard gtx9/*.cpp) \
			 $(wildcard berlin_a/*.cpp)


UPDATEOBJ = $(UPDATESRC:.cpp=.o)
PROGNAME = gdixupdate

# need remove the static flag if it is integrated with Chrome OS
# LDFLAGS += -static

all: $(PROGNAME)

$(PROGNAME): $(UPDATEOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(UPDATEOBJ) -o $(PROGNAME)

clean:
	rm -f $(UPDATEOBJ) $(PROGNAME)
