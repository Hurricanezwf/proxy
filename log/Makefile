
root_dir=../..
SAVE_PATH=../obj/
INCLUDE=../common

#create dir to save log.o
$(shell mkdir -p $(SAVE_PATH))

CC=g++

log.o:
	$(CC) -c -I $(INCLUDE) log.cpp


#move obj file to $(SAVE_PATH)
	mv -f $@ $(SAVE_PATH)

clean:
	rm $(SAVE_PATH)/log.o
