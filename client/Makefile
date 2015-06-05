root_dir=..
INCLUDE=-I../common\
		-I../server\
		-I../log\

CFLAGS=-w
CLINK=-lrt -pthread

OBJS=$(wildcard $(root_dir)/obj/*.o)

CC=g++

client:
	$(CC) $(CLINK) -o client $(CFLAGS) $(OBJS) $(INCLUDE) client.cpp

clean:
	rm client
