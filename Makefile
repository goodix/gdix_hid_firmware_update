CXX ?= g++

CXXFLAGS += -Wall

UPDATESRC = main.cpp gtx5.cpp firmware_image.cpp
UPDATEOBJ = $(UPDATESRC:.cpp=.o)
PROGNAME = gdixupdate

all: $(PROGNAME)

$(PROGNAME): $(UPDATEOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(UPDATEOBJ) -o $(PROGNAME)

clean:
	rm -f $(UPDATEOBJ) $(PROGNAME)
