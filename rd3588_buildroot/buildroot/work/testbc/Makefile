all: testbc

testbc: testbc.o
	$(CC) -o testbc testbc.o

clean:
	rm -rf *.o
	rm -rf testbc
	
install:
	$(INSTALL) -D -m 0755 testbc $(TARGET_DIR)/bin

	
