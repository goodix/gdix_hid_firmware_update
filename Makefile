CXX ?= g++

CXXFLAGS += -Wall
UPDATESRC = main.cpp gt_update.cpp firmware_image.cpp \
	gtx2/gtx2.cpp gtx5/gtx5.cpp  gtx3/gtx3.cpp gtx8/gtx8.cpp\
	gtx2/gtx2_firmware_image.cpp gtx5/gtx5_firmware_image.cpp gtx3/gtx3_firmware_image.cpp gtx8/gtx8_firmware_image.cpp\
	gtx2/gtx2_update.cpp gtx5/gtx5_update.cpp gtx3/gtx3_update.cpp gtx8/gtx8_update.cpp

UPDATEOBJ = $(UPDATESRC:.cpp=.o)
PROGNAME = gdixupdate
STATIC_BUILD ?= y
ifeq ($(STATIC_BUILD), y)
	LDFLAGS += -static
endif

all: $(PROGNAME)

$(PROGNAME): $(UPDATEOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(UPDATEOBJ) -o $(PROGNAME)

clean:
	rm -f $(UPDATEOBJ) $(PROGNAME)
