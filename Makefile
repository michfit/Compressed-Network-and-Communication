#NAME:Michelle Su
#EMAIL: xuehuasu@gmail.com
#ID: 404804135
CC=gcc
CFLAGS= -Wall -Wextra
#FILE=

default: lab1b-client.c lab1b-server.c  
	$(CC) $(CFLAGS) -lz -o lab1b-client lab1b-client.c 
	$(CC) $(CFLAGS) -lz -o lab1b-server lab1b-server.c 
lab1b-client: client.c
	$(CC) $(CFLAGS)  -lz -o lab1b-client lab1b-client.c 
lab1b-server: server.c
	$(CC) $(CFLAGS)  -lz -o lab1b-server lab1b-server.c 
#simple: simple.c
#	$(CC) $(CFLAGS) -o simple -lz -g simple.c
clean: 
	rm -f lab1b-client lab1b-server *OUT LOG* *.OUT *ERR  
dist:
	tar -czf lab1b-404804135.tar.gz Makefile README lab1b-client.c lab1b-server.c

