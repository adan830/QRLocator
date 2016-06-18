
CC=cc
LD=cc

CINCS=-I../opencv/include -I.
CPPFLAGS=-g -Wall

LDINCS=-L../opencv/lib
LDFLAGS=-lpng -lopencv_imgproc -lopencv_highgui -lopencv_core -lopencv_imgcodecs -lopencv_videoio -lstdc++ -Wall

SRCS=locator.o 
OBJS=$(patsubst %cpp, %o, $(SRCS))

all :qrcamera  qrimage

qrcamera: $(OBJS) qrcamera.o
	$(LD) -o qrcamera $^ $(LDINCS) $(LDFLAGS)

qrimage: $(OBJS) qrimage.o
	$(LD) -o qrimage $^ $(LDINCS) $(LDFLAGS)

.PHONY : tags
tags : 
	ctags -R *.cpp
	cscope -Rqb *.cpp

%.o : %.cpp
	$(CC) -c $(CPPFLAGS) $(CINCS) -o $@ $<

clean:
	-rm -f qrimage qrcamera
	-rm -f $(OBJS)
	-rm -f *.o
	-rm -f cscope.* tags


