ifeq ($(arm),true)
	CXX = arm-linux-gnueabi-g++
	TARGETNAME = gdixupdate_arm
else
	CXX = g++
	TARGETNAME = gdixupdate
endif
CXXFLAGS += -Wall
CXXFLAGS += -g -static
UPDATESRC = main.cpp gt_update.cpp firmware_image.cpp \
	gtx2/gtx2.cpp gtx5/gtx5.cpp  gtx3/gtx3.cpp\
	gtx2/gtx2_firmware_image.cpp gtx5/gtx5_firmware_image.cpp gtx3/gtx3_firmware_image.cpp\
	gtx2/gtx2_update.cpp gtx5/gtx5_update.cpp gtx3/gtx3_update.cpp

UPDATEOBJ = $(UPDATESRC:.cpp=.o)
#PROGNAME = gdixupdate


all: $(TARGETNAME)

$(TARGETNAME): $(UPDATEOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(UPDATEOBJ) -o $(TARGETNAME)

clean:
	rm -f $(UPDATEOBJ) $(TARGETNAME)
