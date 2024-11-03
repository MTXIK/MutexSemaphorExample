CC = gcc
CFLAGS = -Wall -Wextra -O2

all: mutex semaphore

mutex: ./src/mutex_baboon.c
	$(CC) $(CFLAGS) -o mutex ./src/mutex_baboon.c

semaphore: ./src/semaphore_baboon.c
	$(CC) $(CFLAGS) -o semaphore ./src/semaphore_baboon.c

clean:
	rm -f mutex semaphore
