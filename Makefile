CC=g++

#server: server.o log.o
#	$(CC) -o server -w server.o log.o

server:base.h server.h server.cpp log.o
	$(CC) -o server -g -w  server.cpp log.o

log.o:base.h log.h log.cpp
	$(CC) -o log.o -c -g -w log.cpp



clean:
	rm *.o server
