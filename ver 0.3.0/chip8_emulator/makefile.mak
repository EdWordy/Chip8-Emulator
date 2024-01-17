CFLAGS=-std=c17 -Wall -Wextra -Werror
LIBS=E:/Dev/SDL2-devel-2.28.5-mingw/SDL2-2.28.5/x86_64-w64-mingw32/lib -lmingw32 -lSDL2main -lSDL2
INCLUDE=E:/Dev/SDL2-devel-2.28.5-mingw/SDL2-2.28.5/x86_64-w64-mingw32/include/SDL2/

all:
	gcc chip8.c -o chip8 $(CFLAGS) -L$(LIBS) -I$(INCLUDE)

debug:
	gcc chip8.c -o chip8 $(CFLAGS) -L$(LIBS) -I$(INCLUDE) -DDEBUG