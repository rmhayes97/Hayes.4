CC=gcc
CFLAGS=-Wall -O2 -g -std=c99 -std=gnu99

all: oss user

oss: oss.c
	$(CC) $(CFLAGS) oss.c -o oss

user: user.c
	$(CC) $(CFLAGS) user.c -o user

clean:
	rm -rf oss user log.txt 

