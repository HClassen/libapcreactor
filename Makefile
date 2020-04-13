CC= gcc
CFLAGS= -W -Wextra -Wall -std=c99
DEBUG= -DDEBUG -g
FILES= src/reactor.c

main: $(FILES)
	$(CC) $(CFLAGS) -o reactor main.c $(FILES)

debug: $(FILES)
	$(CC) $(CFLAGS) -o reactor main.c $(FILES) $(DEBUG)

client: client.c
	$(CC) $(CFLAGS) -o client client.c

lib: libprep libcomb libclean

libprep: $(FILES)
	$(CC) $(CFLAGS) -c $(FILES) -fPIC

libcomb: 
	gcc -shared -o libapcreactor.so  *.o  -lm
	#ar -cvq libapc.a *.o

libclean:
	rm *.o