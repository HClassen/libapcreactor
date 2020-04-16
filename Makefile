CC= gcc
CFLAGS= -W -Wextra -Wall -std=c99
DEBUG= -DDEBUG -g
FILES= src/reactor.c
LIB= -L./ -lapcreactor

main: $(FILES)
	$(CC) $(CFLAGS) -o reactor main.c $(FILES)

debug: $(FILES)
	$(CC) $(CFLAGS) -o reactor main.c $(FILES) $(DEBUG)

examples: client server monitor_stdin

clean:
	rm client server monitor_stdin

client:
	$(CC) $(CFLAGS) -o client examples/client.c $(LIB)

server:
	$(CC) $(CFLAGS) -o server examples/server.c $(LIB)

monitor_stdin:
	$(CC) $(CFLAGS) -o monitor_stdin examples/monitor_stdin.c $(LIB)

lib: libprep libcomb libclean

libprep: $(FILES)
	$(CC) $(CFLAGS) -c $(FILES)

libcomb: 
	ar -cvq libapcreactor.a *.o

libclean:
	rm *.o