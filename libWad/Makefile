all: libWad.a

Wad.o: Wad.cpp Wad.h
	g++ -std=c++17 -g -c Wad.cpp -o Wad.o

libWad.a: Wad.o
	ar cr libWad.a Wad.o

clean:
	rm -f Wad.o libWad.a
