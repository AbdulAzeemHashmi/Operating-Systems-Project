CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: dispatcher ingester processor reporter

dispatcher: src/dispatcher.c
	$(CC) $(CFLAGS) -o dispatcher src/dispatcher.c

ingester: src/ingester.c
	$(CC) $(CFLAGS) -o ingester src/ingester.c

processor: src/processor.c
	$(CC) $(CFLAGS) -o processor src/processor.c

reporter: src/reporter.c
	$(CC) $(CFLAGS) -o reporter src/reporter.c

clean:
	rm -f dispatcher ingester processor reporter
