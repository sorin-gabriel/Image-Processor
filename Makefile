headers = bmp_header.h
sources = bmp.c
rules = -g -Wall -Wextra
libraries = -lm

build:
	gcc $(sources) $(libraries) $(rules) -o bmp
clean:
	rm bmp
run:
	./bmp
