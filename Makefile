#
# Makefile for:
# Programming Assignment B
# Part 1
#
LIBS	=
CFLAGS	= -g

all:	tcp 

#
# Internet stream version (TCP protocol).
#

tcp:	server client directory

client.o server.o: inet.h

server:	server.o 
	gcc $(CFLAGS) -o server server.c -lpthread $(LIBS)

client:	client.o 
	gcc $(CFLAGS) -o client client.c $(LIBS)
directory:
	gcc $(CFLAGS) -o direct directory.c $(LIBS)

#
# Clean up the mess we made
#
clean:
	rm *.o \
	server client direct
