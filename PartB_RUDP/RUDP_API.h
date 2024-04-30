#pragma once
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// #define _DEBUG

/*
 * Defines:
*/
#define RUDP_MAX_PACKET_SIZE 576        // Sources: RFC 791, RFC 1122, RFC 2460

// !!! We decided to remove the timeout (its faster this way), so we raised the MAX_RETRIES to give the ack a chance to arrive.
// !!! It does consume more bandwidth so if thats important we can raise TIMEOUT_USEC to 10000.
// !!! I tested the speeds with various timeouts and 0 performed the best but we decided to leave this option here
/* Recommended values for best performance: (Big loss: the timeout should be as low as possible and max_retries as big as possible)
 * 0% loss: 5 retries, 1 timeout_sec
 * 2% loss: 100 retries, 1000 timeout_usec
 * 5% loss: 400 retries, 100 timeout_usec
 * 10% loss: 500 retries, 15 timeout_usec
 * 50% loss: 1000 retries, 15 timeout_usec
 * 75% loss: 10000 retries, 15 timeout_usec
*/
// #define MAX_RETRIES 10000
// #define TIMEOUT_SEC 0
// #define TIMEOUT_USEC 100
// Changes them to static and placed it into RUDP_API.c so they can be modified during the run according to the calculated packet loss

#define SERVER 1
#define CLIENT 0

#define EXIT_MESSAGE 0      // Exit message is sending 0 as of "we have 0 bytes to send"
#define MB 1048576

#define FAIL 1

/*
 * API Functions:
*/

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
int rudp_socket(struct sockaddr_in *my_addr, int peer_type, uint16_t *seq_number);

/* 
 * @brief Sending data to the peer. Waits for an acknowledgement package, if not received, resends the data.
 * @param 
 * @return 
*/
int rudp_send(int sock_id, void *data, size_t data_size, int flags, struct sockaddr_in *to, uint16_t* seq_number);

/* 
 * @brief Receives data from peer.
 * @param 
 * @return 
*/
int rudp_recv(int sock, void * data, size_t data_size, struct sockaddr_in *client_addr, uint16_t* seq);

/* 
 * @brief Closes a connection between peers.
 * @param 
 * @return 
*/
void rudp_close(int sock);

// A function that attemps to improve performance according to the calculated packet loss.
// returns packet loss
float loss_optimization();
