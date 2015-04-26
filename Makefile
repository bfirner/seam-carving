#Simple non-error checking makefile

carve: src/carve.cpp
	g++ src/carve.cpp -std=c++11 `sdl2-config --cflags --libs` -o carve
