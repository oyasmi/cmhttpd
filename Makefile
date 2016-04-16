P=cmhttpd
OBJECTS=map.o parse.o
CFLAGS= -Wall -g -std=gnu11

$(P): $(OBJECTS)

clean:
	rm $(OBJECTS) $(P)
