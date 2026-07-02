CC=gcc

CFLAGS=-Wall -g

TARGET=acttool

SOURCES=acttool.c
 
all: $(TARGET)
 
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^
 
clean:
	rm -f $(TARGET)
