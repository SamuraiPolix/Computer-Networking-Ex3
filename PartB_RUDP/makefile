CC = gcc

CFLAGS = -Wall -g

DEPS = RUDP_API.h

API_OBJECT = RUDP_API.o

.PHONY: all clean

all: RUDP_Receiver RUDP_Sender

RUDP_Receiver: RUDP_Receiver.o $(API_OBJECT)
	$(CC) $(CFLAGS) -o $@ $^

RUDP_Sender: RUDP_Sender.o $(API_OBJECT)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $@ $^

clean:
	rm -f RUDP_Receiver RUDP_Sender *.o *.h.gch