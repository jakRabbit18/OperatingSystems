#makefile for CS3013 project 3

all: project3

project3_basic: proj3.C
	g++ -o project3_basic proj3.C -pthread

project3: proj3_extra.C
	g++ -o project3 proj3_extra.C -pthread

clean: 
	rm -f project3_extra project3_basic project3

