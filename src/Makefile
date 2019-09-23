CC=gcc
SRC=util.c core.c interpreter.c tui.c main.c
OBJ=tm
CFLAGS=-Wall
LIB=-largp -lncurses

release:
	$(CC) -o $(OBJ) $(SRC) $(LIB) $(CFLAGS) -Ofast

debug:
	$(CC) -o $(OBJ)-debug $(SRC) $(LIB) $(CFLAGS) -g

clean:
	rm $(OBJ) $(OBJ)-debug

demo:
	./$(OBJ) ../examples/double.dtm -s8
	sleep 2
	./$(OBJ) ../examples/add.dtm --tape=../examples/test.tape --frame
	sleep 2
	time ./$(OBJ) ../examples/busy_beaver/b2_5.dtm --fast

.PHONY: release debug clean demo
