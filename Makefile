#Simple non-error checking makefile

carve: src/carve.cpp
	g++ src/carve.cpp -std=c++11 `sdl2-config --cflags --libs` -O3 -o carve

clean:
	rm carve
