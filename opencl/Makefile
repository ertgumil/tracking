CC=gcc

FILE=main

all:
	$(CC) -c -Wall -I /usr/local/cuda/include/ $(FILE).c -o $(FILE).o -O3
	$(CC) $(FILE).o -o $(FILE) -L /usr/local/cuda/lib64/ -l OpenCL -O3
clean:
	$(RM) $(FILE) $(FILE).o
