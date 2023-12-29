TARGET = ww
CC     = gcc
CFLAGS = -g -std=c99 -D_DEFAULT_SOURCE -Wall -Wvla -Werror -fsanitize=address,undefined -pthread

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(TARGET) *.o *.a *.dylib *.dSYM