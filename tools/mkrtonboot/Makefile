CC=gcc

CFLAGS=-Wall -g

TARGET=mkrtonboot

SOURCES=mkrtonboot.c
 
all: $(TARGET)
 
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^
 
clean:
	rm -f $(TARGET)
