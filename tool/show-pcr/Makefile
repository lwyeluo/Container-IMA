CXXFLAGS =	-O2 -g -Wall -fmessage-length=0

OBJS =		show-pcr.o

LIBS =	-ltspi

TARGET =	show-pcr

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	
install:
	cp $(TARGET) /usr/bin
