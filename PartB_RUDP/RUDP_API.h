#pragma once
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define SERVER 1
#define CLIENT 0



/*
 * This file contain all the fuctions of the RUDP PROTOCOL that need to be implemented by RUDP_API.c,
 * (That is, to prevent reusing the same chuck of code both in the Sender and the Receiver.). 
 * see a draft of RUDP in this link: https://datatracker.ietf.org/doc/html/draft-ietf-sigtran-reliable-udp-00/ .
 * This RUDP Protocol supports IPv4 ONLY!
*/

/* 
 * @brief Creates an RUDP socket and a handshake between two peers.
 * @param struct sockaddr_in* and the peer type (CLIENT or SERVER);
*/
int rudp_socket(struct sockaddr_in *my_addr, int peer_type, int *seq_number);

/* 
 * @brief Sending data to the peer. Waits for an acknowledgement package, if not received, resends the data.
 * @param 
 * @return 
*/
int rudp_send(int sock_id, const void *data, size_t data_size, int flags, struct sockaddr_in *to, uint8_t seq_number);

/* 
 * @brief Receives data from peer.
 * @param 
 * @return 
*/
// int rudp_recv();

/* 
 * @brief Closes a connection between peers.
 * @param 
 * @return 
*/
// int rudp_close();