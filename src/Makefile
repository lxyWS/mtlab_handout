CC = gcc
CXX = g++

all: histogram histo-private histo-lockfree histo-lock1 histo-lock2

ppmb_io.o: ppmb_io.c ppmb_io.h
	$(CC) -O3 -c ppmb_io.c

ppmb_io.a: ppmb_io.o
	ar rs $@ $<

histogram: histogram.cpp ppmb_io.a
	$(CXX) -O3 $^ -o $@ -lm -lrt

histo-private: histo-private.cpp ppmb_io.a
	$(CXX) -O3 $^ -o $@ -lm -lrt -pthread

histo-lockfree: histo-lockfree.cpp ppmb_io.a
	$(CXX) -O3 $^ -o $@ -lm -lrt -pthread

histo-lock1: histo-lock1.cpp ppmb_io.a
	$(CXX) -O3 $^ -o $@ -lm -lrt -pthread

histo-lock2: histo-lock2.cpp ppmb_io.a
	$(CXX) -O3 $^ -o $@ -lm -lrt -pthread

clean:
	rm -f ppmb_io.a ppmb_io.o histogram histo-private histo-lockfree histo-lock1 histo-lock2 *.hist
