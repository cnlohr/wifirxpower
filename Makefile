all : wifirx

LDFLAGS:=-lX11 -lm -lpthread -lXinerama -lXext

wifirx : wifirx.o XDriver.o DrawFunctions.o
	gcc -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ wifirx
