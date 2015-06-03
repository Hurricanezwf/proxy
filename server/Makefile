
root_dir=..

INCLUDE=-I../common/\
		-I../log/\

OBJS=$(wildcard $(root_dir)/obj/*.o)

CC=g++
CFLAGS=-w -g
CLINK=-lrt
server:
	$(CC) $(CLINK) -o server $(CFLAGS) $(INCLUDE) $(OBJS)  server.cpp


clean:
	rm  server
