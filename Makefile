CC=gcc
CFLAGS=-I. -g -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse
LIBS = -luuid -lfuse -pthread -lm
DEPS = myfs_lib.h myfs.h fs.h unqlite.h
OBJ = unqlite.o fs.o myfs_lib.o
TARGET = myfs

all: $(TARGET)

tests: test/create_dir test/create_file test/update_file

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(TARGET).o $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

test/%: test/%.o $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o *~ core myfs.db myfs.log $(TARGET)
