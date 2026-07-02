all: testbc

mkpreactimg: mkpreactimg.o
	$(CC) -o testbc testbc.o

clean:
	rm -rf *.o
	rm -rf mkpreactimg
	
install:
	$(INSTALL) -D -m 0755 mkpreactimg $(TARGET_DIR)/bin

	
