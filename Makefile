CXX ?= g++

CXXFLAGS += -Wall
#-O2 -s -pedantic -std=c++2a
UPDATESRC := $(wildcard *.cpp) \
			 $(wildcard gtx2/*.cpp) \
			 $(wildcard gtx3/*.cpp) \
			 $(wildcard gtx5/*.cpp) \
			 $(wildcard gtx8/*.cpp) \
			 $(wildcard gtx9/*.cpp)


UPDATEOBJ = $(UPDATESRC:.cpp=.o)
PROGNAME = gdixupdate

# LDFLAGS += -static

all: $(PROGNAME)

$(PROGNAME): $(UPDATEOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(UPDATEOBJ) -o $(PROGNAME)

clean:
	rm -f $(UPDATEOBJ) $(PROGNAME)
