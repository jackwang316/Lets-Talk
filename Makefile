all: lets-talk.c
		gcc -o lets-talk lets-talk.c list.c -ggdb3 -lpthread
		valgrind ./lets-talk 3000 127.0.0.1 3001

clean:
		$(RM) lets-talk
