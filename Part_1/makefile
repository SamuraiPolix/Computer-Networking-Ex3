CC = gcc

CFLAGS = -Wall -g

.PHONY: all clean

all: TCP_Receiver TCP_Sender

TCP_Receiver: TCP_Receiver.c
	$(CC) $(CFLAGS) -o $@ $^

TCP_Sender: TCP_Sender.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f TCP_Receiver TCP_Sender *.o *.h.gch