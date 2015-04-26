
root_dir=..

INCLUDE=-I../common/\
		-I../log/\

OBJS=$(wildcard $(root_dir)/obj/*.o)

CC=g++
CFLAGS=-w -g

server:
	$(CC) -o server $(CFLAGS) $(INCLUDE) $(OBJS)  server.cpp


clean:
	rm  server
