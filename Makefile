CC=gcc
SOURCE= main.c find/libfind.a
TARGET=ff
CFLAGS=-g -Wall -Werror -pthread 

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -rf $(TARGET)
