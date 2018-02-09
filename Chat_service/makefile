# makefile

all: server client

server: crsd.c
	g++ -g -std=c++11 -pthread -o server crsd.c

client: crc.c
	g++ -g -std=c++11 -o client crc.c

clean:
	rm -rf server client *.o