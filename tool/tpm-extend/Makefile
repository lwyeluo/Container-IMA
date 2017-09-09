CXXFLAGS =	-O2 -g -Wall -fmessage-length=0

OBJS =		tpm-extend.o

LIBS =	-ltspi

TARGET =	tpm-extend

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	
install:
	cp $(TARGET) /usr/bin
