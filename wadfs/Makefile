all: wadfs

wadfs: wadfs.cpp
	g++ -std=c++17 -g -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 wadfs.cpp -o wadfs -L ../libWad -lWad -lfuse

clean:
	rm -f wadfs
