C = gcc
CFLAG = -g -I -std=c99 -std=gnu99 -D_SVID_SOURCE -D_POSIX_C_SOURCE

all: oss user

%.o: %.c
	$(CC) $(CFLAG) -c  $< -o $@ -lm

oss: oss.o
	$(CC) $(CFLAG) $< -o $@ -lm

user: user.o
	$(CC) $(CFLAG) $< -o $@ -lm

clean:
	rm -f *.o oss user cstest logfile.*

