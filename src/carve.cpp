/*
 *
 * Seam carving written in C++
 *
 * Compile like this:
 * g++ carve.cpp -std=c++11 `sdl2-config --cflags --libs`
 *
 *
 The MIT License (MIT)

 Copyright (c) 2015 Bernhard Firner

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 *
 *
 *
 */

#include <algorithm>
#include <iostream>
#include <iterator>
#include <functional>
#include <future>
#include <vector>
#include <SDL2/SDL.h>
#include <cmath>


//TODO FIXME This has to be 4 bytes to work
//Just making that assumption for now
struct RGB {
	uint8_t alpha;
	uint8_t blue;
	uint8_t green;
	uint8_t red;
};

//Simple integer square
unsigned int square(unsigned int val) {
 return val*val;
} 

//Skipping doing square root to make the difference computation faster
unsigned int operator-(RGB& a, RGB& b) {
	return square((unsigned int)a.red - b.red) +
	       square((unsigned int)a.green - b.green) +
	       square((unsigned int)a.blue - b.blue);
}

//Stupid 24 byte format
struct ThreeBytes {
	uint8_t bytes[3];
};

struct SeamValue {
	unsigned int cost;
	int prev;
};

bool operator<(SeamValue& a, SeamValue& b) {
	return a.cost < b.cost;
}

std::vector<uint32_t> getVerticalSeam(std::vector<RGB>& pixels, uint32_t width, uint32_t height) {
	//Initialized path costs
	std::vector<std::vector<SeamValue>> paths(height, {width, {0, -1}});
	//Calculate the minimum path costs
	for (int h = 1; h < height; ++h) {
		//Now loop over this row and update the paths
		//Update in two different threads, 0 to width/2 and width/2 to width
		std::function<bool(int, int)> update = [&](int start, int stop) {
			for (int x = start; x < stop; ++x) {
				//Check each of the possible sources
				for (int i = -1; i < 2; ++i) {
					int source = x+i;
					uint32_t here = h*width + x;
					SeamValue& cost_here = paths[h][x];
					if (0 <= source and source < width) {
						//Calculate the cost
						uint32_t prev = (h-1)*width + source;
						unsigned int delta = pixels[here] - pixels[prev];
						//If there isn't a path here yet or this is the least

						SeamValue& cost_prev = paths[h-1][source];
						if (-1 == cost_here.prev or cost_prev.cost+delta < cost_here.cost) {
							cost_here.prev = source;
							cost_here.cost = cost_prev.cost+delta;
						}
					}
				}
			}
			return true;
		};
		//Run the operation on the right side asynchronously
		//Use a future to wait for the right side to finish
		std::future<bool> right = std::async(std::launch::async, update, width/2, width);
		//Run the left side
		update(0, width/2);
		//Wait for the right side to finish
		right.get();
	}
	//The minimum path
	auto min = std::min_element(paths.at(height-1).begin(), paths.at(height-1).end());
	std::vector<uint32_t> min_path(height, 0);
	min_path.back() = std::distance(paths.back().begin(), min);
	int prev = min->prev;
	for (int h = height-1; h > 0; --h) {
		//Set the offset at this height
		min_path.at(h-1) = prev;
		prev = paths.at(h-1).at(prev).prev;
	}
	
	return min_path;
}

std::vector<RGB> removeVerticalSeam(const std::vector<uint32_t>& seam, std::vector<RGB>& pixels, uint32_t width, uint32_t height) {
	std::vector<RGB> new_pixels(height*(width-1));
	for (int h = 0; h < height; ++h) {
		//Copy this row, skipping the removed pixel
		auto begin = pixels.begin() + (h*width);
		auto end = begin + width;
		std::copy(begin, begin + seam.at(h), new_pixels.begin()+h*(width-1));
		std::copy(begin + seam.at(h)+1, end, new_pixels.begin()+h*(width-1)+seam.at(h));
	}
	return new_pixels;
}

std::vector<uint32_t> getHorizontalSeam(std::vector<RGB>& pixels, uint32_t width, uint32_t height) {
	//Initialized path costs
	std::vector<std::vector<SeamValue>> paths(width, {height, {0, -1}});
	//Calculate the minimum path costs from left to right
	for (int w = 1; w < width; ++w) {
		//Now loop over this column and update the paths
		std::function<bool(int, int)> update = [&](int start, int stop) {
			for (int y = start; y < stop; ++y) {
				//Check each of the possible sources (left-down, left, and left-up
				for (int i = -1; i < 2; ++i) {
					int source = y+i;
					uint32_t here = w + y*width;
					SeamValue& cost_here = paths[w][y];
					if (0 <= source and source < height) {
						//Calculate the cost
						uint32_t prev = (w-1) + source*width;
						unsigned int delta = pixels[here] - pixels[prev];
						//If there isn't a path here yet or this is the least

						SeamValue& cost_prev = paths[w-1][source];
						if (-1 == cost_here.prev or cost_prev.cost+delta < cost_here.cost) {
							cost_here.prev = source;
							cost_here.cost = cost_prev.cost+delta;
						}
					}
				}
			}
			return true;
		};
		//Run the operation on the lower side asynchronously
		//Use a future to wait for the lower side to finish
		std::future<bool> lower = std::async(std::launch::async, update, height/2, height);
		//Run the left side
		update(0, height/2);
		//Wait for the lower side to finish
		lower.get();
	}
	//The minimum path
	auto min = std::min_element(paths.at(width-1).begin(), paths.at(width-1).end());
	std::vector<uint32_t> min_path(width, 0);
	min_path.back() = std::distance(paths.back().begin(), min);
	int prev = min->prev;
	for (int w = width-1; w > 0; --w) {
		//Set the offset at this width
		min_path.at(w-1) = prev;
		prev = paths.at(w-1).at(prev).prev;
	}
	
	return min_path;
}

std::vector<RGB> removeHorizontalSeam(const std::vector<uint32_t>& seam, std::vector<RGB>& pixels, uint32_t width, uint32_t height) {
	std::vector<RGB> new_pixels((height-1)*width);
	//TODO FIXME Copying like this is inefficient - is there an elegant and
	//efficient alternative?
	for (int w = 0; w < width; ++w) {
		//The insertion index
		size_t ins_index = w;
		for (int h = 0; h < height; ++h) {
			//Copy all pixels except for the removed ones
			if (h != seam.at(w)) {
				new_pixels.at(ins_index) = pixels.at(h*width + w);
				//The insertion index jumps to the next row (1 width away)
				ins_index += width;
			}
		}
	}
	return new_pixels;
}

int main(int argc, char** argv) {

	if (argc != 2) {
		std::cout<<"This program needs a filename to open!\n";
		return 0;
	}

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) != 0){
		std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	std::string imagePath = std::string(argv[1]);
	SDL_Surface *bmp = SDL_LoadBMP(imagePath.c_str());
	if (bmp == nullptr){
		std::cout << "SDL_LoadBMP Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}

	std::cout<<"The width and height of the image are "<<
		bmp->w<<" and "<<bmp->h<<'\n';
	uint32_t width = bmp->w;
	uint32_t height = bmp->h;
	uint32_t cur_width = bmp->w;
	uint32_t cur_height = bmp->h;
	uint32_t bpp = (unsigned int)bmp->format->BitsPerPixel;
	std::cout<<"Bits per pixel is "<<(unsigned int)bmp->format->BitsPerPixel<<'\n';
	std::cout<<"Pitch is "<<(unsigned int)bmp->pitch<<'\n';
	//Lock the surface and store the pixels into a vector
	//The vector is going to be flat: all pixels in just one row
	unsigned int total_pixels = bmp->h * bmp->w;
	std::vector<RGB> orig_pixels(total_pixels);
	SDL_LockSurface(bmp);
	{
		//The pixels of the loaded image
		ThreeBytes* pixels = ((ThreeBytes*)bmp->pixels);

		std::function<RGB(ThreeBytes&)> pixelToRGB = [bmp] (ThreeBytes& bytes) {
			uint32_t temp;
			uint32_t pixel = *reinterpret_cast<uint32_t*>(&bytes);
			//For each color we will use the mask to
			//get only the relevant pixels, then
			//shift them down to an 8 bit value,
			//then shift up to a full 8 bit range

			// Get Red component
			temp = pixel & bmp->format->Rmask;
			temp = temp >> bmp->format->Rshift;
			temp = temp << bmp->format->Rloss;
			uint8_t red = temp;

			// Get Green component
			temp = pixel & bmp->format->Gmask;
			temp = temp >> bmp->format->Gshift;
			temp = temp << bmp->format->Gloss;
			uint8_t green = temp;

			// Get Blue component
			temp = pixel & bmp->format->Bmask;
			temp = temp >> bmp->format->Bshift;
			temp = temp << bmp->format->Bloss;
			uint8_t blue = temp;
			//return RGB{0xFF, blue, green, red};
			uint8_t avg = ((uint32_t)blue + (uint32_t)green + (uint32_t)red)/3.0;
			return RGB{0xFF, blue, green, red};
		};

		//Transform the SDL pixel into my RGBA pixel
		std::transform(pixels, pixels+total_pixels, orig_pixels.begin(), pixelToRGB);
	}
	SDL_FreeSurface(bmp);
	//Find the seams and color the pixels
	SDL_UnlockSurface(bmp);
	//Copy that will store the modified pixels
	std::vector<RGB> cur_pixels(orig_pixels);

	//Now try making a surface from the vector
	//(The pixel data is not copied, must free this before the vector)
	SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(orig_pixels.data(), width, height, 8*sizeof(RGB), sizeof(RGB)*width, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);

	if (surface == nullptr) {
		fprintf(stderr, "CreateRGBSurface failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}


	//Create a window with title "Hello World!" and the size for the image
	SDL_Window *win = SDL_CreateWindow("Hello World!", 100, 100, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (win == nullptr){
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		SDL_FreeSurface(surface);
		SDL_Quit();
		return 1;
	}

	//Set up the renderer
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == nullptr){
		SDL_DestroyWindow(win);
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_FreeSurface(surface);
		SDL_Quit();
		return 1;
	}

	//Create a texture from the surface
	SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surface);
	//SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, bmp);
	if (tex == nullptr){
		SDL_FreeSurface(surface);
		SDL_DestroyRenderer(ren);
		SDL_DestroyWindow(win);
		std::cout << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}

	//e is an SDL_Event variable we've declared before entering the main loop
	SDL_Event e;
	bool quit = false;
	while (not quit) {
		while (SDL_PollEvent(&e)){
			//Remember the previous dimensions if we resize
			uint32_t last_width = cur_width;
			uint32_t last_height = cur_height;
			switch (e.type) {
				//If user closes the window
				case SDL_QUIT:
					quit = true;
					break;
				case SDL_WINDOWEVENT:
					switch (e.window.event) {
						case SDL_WINDOWEVENT_RESIZED:
							//Grew larger? Start from scratch
							if (e.window.data1 > cur_width or
									e.window.data2 > cur_height) {
								cur_pixels = orig_pixels;
								last_width = width;
								last_height = height;
							}
							cur_width = e.window.data1;
							cur_height = e.window.data2;
							SDL_SetWindowSize(win, cur_width, cur_height);
							std::cout<<"Resizing to "<<cur_width<<", "<<cur_height<<'\n';

							//If we've gotten narrower then delete some vertical seams
							if (cur_width < last_width) {
								for (int i = 0; (last_width-i) > cur_width; ++i) {
									//Find the seam
									std::vector<unsigned int> seam = getVerticalSeam(cur_pixels, last_width-i, last_height);
									//And remove it
									cur_pixels = removeVerticalSeam(seam, cur_pixels, last_width-i, last_height);
								}
							}

							//If we've gotten shorter then delete some horizontal seams
							if (cur_height < last_height) {
								for (int i = 0; (last_height-i) > cur_height; ++i) {
									//Find the seam
									std::vector<unsigned int> seam = getHorizontalSeam(cur_pixels, cur_width, last_height-i);
									//And remove it
									cur_pixels = removeHorizontalSeam(seam, cur_pixels, cur_width, last_height-i);
								}
							}
							std::cerr<<"Destroying surface and redrawing\n";

							//Destroy the existing texture and surface to be remade
							SDL_FreeSurface(surface);
							SDL_DestroyTexture(tex);

							//Now try making a surface from the vector
							//(The pixel data is not copied, must free this before the vector)
							surface = SDL_CreateRGBSurfaceFrom(cur_pixels.data(), cur_width, cur_height, 8*sizeof(RGB), sizeof(RGB)*cur_width, 0xFF000000, 0xFF0000, 0xFF00, 0xFF);

							if (surface == nullptr) {
								SDL_DestroyRenderer(ren);
								SDL_DestroyWindow(win);
								fprintf(stderr, "CreateRGBSurface failed: %s\n", SDL_GetError());
								SDL_Quit();
								return 1;
							}

							//Create a texture from the surface
							tex = SDL_CreateTextureFromSurface(ren, surface);
							//SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, bmp);
							if (tex == nullptr){
								SDL_FreeSurface(surface);
								SDL_DestroyRenderer(ren);
								SDL_DestroyWindow(win);
								std::cout << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << std::endl;
								SDL_Quit();
								return 1;
							}
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
		}
		//First clear the renderer
		SDL_RenderClear(ren);
		//Draw the texture
		SDL_RenderCopy(ren, tex, NULL, NULL);
		//Update the screen
		SDL_RenderPresent(ren);
		//Don't poll too quickly and beat up the CPU
		SDL_Delay(10);
	}

	//Free the surface, get rid of the texture, rendered, etc
	SDL_FreeSurface(surface);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return 0;
}
