all : wifirx

LDFLAGS:=-lX11 -lm -lpthread -lXinerama -lXext


%.o: %.c
	gcc -c -o $@ $< -g3 -O0
	
wifirx : wifirx.o XDriver.o DrawFunctions.o
	gcc -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ wifirx
