root_dir=..
INCLUDE=-I../common\
		-I../server\
		-I../log\

CFLAGS=-w

OBJS=$(wildcard $(root_dir)/obj/*.o)

CC=g++

client:
	$(CC) -o client $(CFLAGS) $(OBJS) $(INCLUDE) client.cpp

clean:
	rm client
