
CC=cc
LD=cc

CINCS=-I../opencv/include -I.
CPPFLAGS=-g -Wall

LDINCS=-L../opencv/lib
LDFLAGS=-lpng -lopencv_imgproc -lopencv_highgui -lopencv_core -lopencv_imgcodecs -lstdc++ -Wall

SRCS=display_image.o
OBJS=$(patsubst %cpp, %o, $(SRCS))

all : png tags

#png : $(OBJS) png_test.o
png : $(OBJS)
	$(LD) -o locator $^ $(LDINCS) $(LDFLAGS)

.PHONY : tags
tags : 
	ctags -R *.cpp
	cscope -Rqb *.cpp

%.o : %.cpp
	$(CC) -c $(CPPFLAGS) $(CINCS) -o $@ $<

clean:
	-rm -f locator
	-rm -f $(OBJS)
	-rm -f *.o
	-rm -f cscope.* tags


