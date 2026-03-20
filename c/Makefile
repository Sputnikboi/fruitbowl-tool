CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -Isrc -Iinclude
LDFLAGS = -Llib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRC = src/main.c src/model.c src/pack.c src/renderer.c src/gui.c src/util.c src/filedialog.c lib/cJSON.c
OUT = fruitbowl_tool

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUT)
