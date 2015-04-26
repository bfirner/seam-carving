# seam-carving
Just for fun implementation of the seam carving algorithm

This code implements the seam carving algorithm described in the paper _Seam Carving for Content-Aware Image Resizing_ written by Avidan and Shamir. This implementation isn't particularly great, it was just written written with some friends at WINLAB. A bit of time was spent optimizing the speed using valgrind, and the program uses two threads to speed things up.

# Requirements
This program uses SDL2 to load and display an image. Right now it only works with bmp images with 24-bit pixels. It also only works on big endian systems.
