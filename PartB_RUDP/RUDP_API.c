#include "RUDP_API.h"
#include <stdio.h>

/*
 * This file contain all implementations for the RUDP API functions.
 */

/* RUDP: (built "on top" of the regular UDP)
    0 1 2 3 4 5 6 7 8            15 16                            31 
   +-+-+-+-+-+-+-+-+---------------++-+-+-+-+-+-+-+-+---------------+
   |                               |                                |
   |              Length           |            Checksum            |
   |                               |                                |
   |                               |                                |
   +-+-+-+-+-+-+-+-+---------------++-+-+-+-+-+-+-+-+---------------+
   |  Sequence #   |   Ack Number  ||S|A|E|R|N|C|T| |
   |               |               ||Y|C|A|S|U|H|C|0|
   |               |               ||N|K|K|T|L|K|S| |
   +---------------+---------------+
*/

/*
 * Defines:
*/
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) > (b) ? (a) : (b))

/*
 * Structs:
*/
// UDP header will be created on top of this
typedef struct _flags {
    unsigned int syn : 1;       // indicates a sync segment in present
    unsigned int ack : 1;       // indicates the ack num in the header is valid
    unsigned int eak : 1;       // not used
    unsigned int rst : 1;       // not used
    unsigned int nul : 1;       // indicates a null segment packet
    unsigned int chk : 1;       // 0 - checksum contains header only. 1 - checksum contains header and data.
    unsigned int tcs : 1;       // not used
    unsigned int     : 1;       // not used
} flags_bitfield;

typedef struct _rudp_packet_header {
    uint16_t length;      // length of the data itself, without the RUDP header
    uint16_t checksum;      // used to validate the corectness of the data
    uint16_t seq_ack_number;    // when sending a packet - seq number is stored here. when sending an ack, the ack number is stored here.
    flags_bitfield flags;          // 1 byte unassigned int - used to classify the packet (SYN, ACK, etc.)
} rudp_packet_header;

typedef struct _rudp_packet {
    rudp_packet_header header;          // header, defined above
    char data[RUDP_MAX_PACKET_SIZE - sizeof(rudp_packet_header)];      // data without header
} rudp_packet;

/*
 * Static Consts:
*/
static const int RUDP_MAX_DATA_SIZE = RUDP_MAX_PACKET_SIZE - sizeof(rudp_packet_header);
static int max_tries = 0;
int *b = &max_tries; /* global int pointer, pointing to global static*/

// These are close to the best settings for 0% packet loss. if there is packet loss, the program will change those values to perform the best
static int MAX_RETRIES = 10000;
static int TIMEOUT_SEC = 0;
static int TIMEOUT_USEC = 1000;

// Used to calculate packet loss during the run and adjust timeout and retries
static int packets_sent = 0;
static int ack_received = 0;

/*
 * Declating Functions:
*/
unsigned short int calculate_checksum(void *data, unsigned int bytes);
float calculate_packet_loss();
char* get_packet_type(rudp_packet* packet);
int rudp_send_packet(rudp_packet* packet, int sock_id, struct sockaddr_in *to);
int rudp_send_ack(int sock_id, struct sockaddr_in *to, int seq_number);
int rudp_send_syn(int sock_id, struct sockaddr_in *to, int seq_number);
rudp_packet* create_packet(void *data, size_t data_size, int seq_ack_number);
int rudp_recv_syn(int sock, struct sockaddr_in *client_addr);
int rudp_recv_ack(int sock, struct sockaddr_in *sender_addr);

/* 
 * API Functions
*/
int rudp_socket(struct sockaddr_in *server_address, int peer_type, uint16_t * seq_number)
{
    int sock = -1;

    // create a socket over UDP, with UDP Protocol
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock == -1){
        perror("sock");
        exit(FAIL);
    }

    // if peer_type is SERVER - this is a server that needs binding
    if (peer_type == SERVER) {
        if (bind(sock, (struct sockaddr *)server_address, sizeof(*server_address)) == -1){
            if (errno == EADDRINUSE) {
                // Deal with "Address already in use" error
                fprintf(stderr, "bind: Address already in use\n");
                printf("Fixing error...\n");
                int yes = 1;
                if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1){
                    fprintf(stderr, "Couldn't fix the error, Exiting...\n");
                    perror("setsockopt");
                    close(sock);
                    exit(FAIL);
                }
                printf("Error is fixed.\nPlease restart the program!\n");
                close(sock);
                exit(FAIL);
            }
            else {
                perror("bind");
                close(sock);
                exit(FAIL);
            }
        }
    }

    // Handshake
    printf("Handshake Started.\n");
    if (peer_type == CLIENT) {
        // send SYN to server
        rudp_send_syn(sock, server_address, *seq_number);
        printf("SYN Sent.\n");
        printf("ACK-SYN Received.\n");
    } else if (peer_type == SERVER) {
        struct sockaddr_in client_addr;
        // wait for SYN from client
        *seq_number = rudp_recv_syn(sock, &client_addr);
        printf("SYN Received.\n");
        printf("ACK-SYN Sent.\n");
    }
    *seq_number += 1;
    printf("Handshake completed!\n");
    return sock;
}

int rudp_send(int sock_id, void *data, size_t data_size, int flags, struct sockaddr_in *to, uint16_t* seq_number)
{
    int chunk_size, remaining_bytes;
    int total_bytes_sent = 0, bytes_sent;

    while (total_bytes_sent < data_size){
        // // calculate chunk size
        remaining_bytes = data_size-total_bytes_sent;

        #ifdef _DEBUG
        printf("Remaining to send: %d\n", remaining_bytes);
        #endif

        // spliting large size data into chunks that fit the maximum allowed data size for RUDP
        chunk_size = remaining_bytes < RUDP_MAX_DATA_SIZE ? remaining_bytes : RUDP_MAX_DATA_SIZE; // -1 for '\0'
        
        // create an RUDP simple packet (with current data chunk - total_bytes_sent acts as a pointer)
        rudp_packet* packet = create_packet(data+total_bytes_sent, chunk_size, *seq_number);

        // send chunk
        bytes_sent = rudp_send_packet(packet, sock_id, to);     // actual bytes - data witout header

        #ifdef _DEBUG
        printf("Bytes sent: %d\n", bytes_sent);
        #endif

        total_bytes_sent += bytes_sent;
        *seq_number += 1;
    }

    return total_bytes_sent;
}

int rudp_recv(int sock, void * data, size_t data_size, struct sockaddr_in *client_addr, uint16_t* seq){
    rudp_packet* packet = (rudp_packet *) malloc (sizeof(rudp_packet));

    if (packet == NULL){
        fprintf(stderr, "ERROR: Failed to allocate memory for the packet!\n");
        return 0;
    }
    socklen_t len = sizeof(struct sockaddr_in);

    do{
        int bytes = recvfrom(sock, packet, sizeof(*packet), 0, (struct sockaddr *) client_addr, &len);

        if (bytes <= -1){
            perror("recv");
            close(sock);
            exit(FAIL);
        }
        else if (bytes == 0){
            printf("Connection was closed prior to receiving the data4!\n");
            close(sock);
            exit(FAIL);
        }

        #ifdef _DEBUG
        char* type = get_packet_type(packet);
        printf("Received %spacket, SEQ: %d\n", type, packet->header.seq_ack_number);
        if(packet->header.checksum != calculate_checksum(packet->data, sizeof(packet->data))){
            printf("Checksum doesn't match: Received: %d, Calculated: %d\n", packet->header.checksum, calculate_checksum(packet->data, sizeof(packet->data)));
        }
        #endif

        // Send ACK after receiving only if received packet was not ACK
        if (packet->header.flags.ack != 1){
            rudp_send_ack(sock, client_addr, packet->header.seq_ack_number+1);
        }
        if (*(int*)(packet->data) == 0){    // sender wants to end connection - wait to see if more packet arrive (maybe the ack is lost)
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(sock, &read_fds);

            int ready = select(sock + 1, &read_fds, NULL, NULL, &timeout);
            if (ready == -1) {
                perror("select");
                close(sock);
                exit(FAIL);
            } else if (ready == 0) {
                break;      // no more packets - connection will be closed
            } else {
                continue;       // recv again and resend ack
            }
        }
        if ((packet->header.flags.syn != 1) && *seq == packet->header.seq_ack_number && (packet->header.checksum == calculate_checksum(packet->data, sizeof(packet->data)))){
            break;
        }
        // else // if by accident a syn was given -OR- seq doesnt match -OR- checksum doesnt match === wrong packet, ignore it and keep receiving
    } while (1);
    *seq += 1;

    data_size = packet->header.length;

    if (data != NULL){
        memcpy(data, packet->data, data_size);
    }

    // Received and send ack -> free packet
    free(packet);
    
    return data_size;
}

void rudp_close(int sock){
    close(sock);
}

/*
 * Helepr Functions
*/
int rudp_send_packet(rudp_packet* packet, int sock_id, struct sockaddr_in *to) {
    int bytes_sent;
    int tries = 0;      // count num of tries to get an ack packet back

    // Send packet
    do {    // while ACK not received or timed out
        packets_sent++;
        #ifdef _DEBUG
        printf("Try #%d\n", tries+1);
        #endif

        tries++;
        bytes_sent = sendto(sock_id, packet, sizeof(*packet), 0, (struct sockaddr *) to, sizeof(*to));
        if (bytes_sent == -1) {
            perror("sendto");
            close(sock_id);
            exit(FAIL);
        } else if (bytes_sent == 0) {
            printf("Connection was closed prior to sending the data!\n");
            close(sock_id);
            exit(FAIL);
        }
        bytes_sent = packet->header.length;     // actual data size

        // optimize loss
        float packet_loss = loss_optimization(); 

        #ifdef _DEBUG
        char* type = get_packet_type(packet);
        printf("Sending %spacket, SEQ: %d\n", type, packet->header.seq_ack_number);
        printf("Packet loss; %f\n", packet_loss);
        #endif

        // Wait for new packet - ACK if sent packet was data or SYN, and data if sent packet was ACK
        if (packet->header.flags.ack != 1) {   
            // Check if ACK received
            struct timeval timeout;
            timeout.tv_sec = TIMEOUT_SEC;
            timeout.tv_usec = TIMEOUT_USEC;

            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(sock_id, &read_fds);

            int ready = select(sock_id + 1, &read_fds, NULL, NULL, &timeout);
            if (ready == -1) {
                perror("select");
                close(sock_id);
                exit(FAIL);
            } else if (ready == 0) {
                // Timeout occurred
                #ifdef _DEBUG
                printf("Timeout occurred while waiting for acknowledgment, resending packet\n");
                #endif
                continue;       // resend
            } else {
                // Check ACK received
                int seq = rudp_recv_ack(sock_id, to);
                if (seq == -1 || seq != packet->header.seq_ack_number+1) {
                    // ack doesnt match seq
                    #ifdef _DEBUG
                    printf("ACK doesn't match SEQ, resending...\n");
                    #endif
                    continue;   // resend
                } else {
                    // a good ACK was received
                    break;
                }
            }
        }
        else{
            break;
        }
    } while (tries < MAX_RETRIES || packet->header.length == 4);     // Resend if ACK was not received. Only if this packet is not an ACK

    if (tries > max_tries)
        max_tries = tries;
    if (tries == MAX_RETRIES){
        fprintf(stderr, "ERROR! Exceeded max retries to send packet! exiting...\n");
        close(sock_id);
        exit(FAIL);
    }

    free(packet);


    return bytes_sent;
}


int rudp_send_ack(int sock_id, struct sockaddr_in *to, int seq_number){
    rudp_packet* ack_packet = create_packet(NULL, 0, seq_number);
    // set NUL flag to 1 -> set ACK flag to 1 - this is an ACK packet; following draft guidelines
    ack_packet->header.flags.nul = 1;
    ack_packet->header.flags.ack = 1;

    return rudp_send_packet(ack_packet, sock_id, to);
}

int rudp_send_syn(int sock_id, struct sockaddr_in *to, int seq_number){
    rudp_packet* syn_packet = create_packet(NULL, 0, seq_number);
    // set NUL flag to 1 -> set SYN flag to 1 - this is a SYN packet; following draft guidelines
    syn_packet->header.flags.nul = 1;
    syn_packet->header.flags.syn = 1;

    return rudp_send_packet(syn_packet, sock_id, to);
}

rudp_packet* create_packet(void *data, size_t data_size, int seq_ack_number){
    rudp_packet* packet = (rudp_packet*) malloc (sizeof(rudp_packet));

    if (packet == NULL){
        fprintf(stderr, "ERROR: Failed to allocate memory for the packet!\n");
        return NULL;
    }
    // prepare memory - reset all data in allocated memory
    memset(&packet->header, 0, sizeof(packet->header));
    memset(&packet->data, 0, sizeof(packet->data));

    packet->header.length = data_size;
    packet->header.seq_ack_number = seq_ack_number;

    // copy data
    memcpy(packet->data, data, data_size);
    packet->header.checksum = calculate_checksum(packet->data, sizeof(packet->data));
    return packet;
}

int rudp_recv_packet(int sock, rudp_packet * packet, size_t packet_size, struct sockaddr_in *client_addr){
    socklen_t len = sizeof(struct sockaddr_in);
    recvfrom(sock, packet, sizeof(*packet), 0, (struct sockaddr *) client_addr, &len);

    #ifdef _DEBUG
    char* type = get_packet_type(packet);
    printf("Received %spacket, SEQ: %d\n", type, packet->header.seq_ack_number);
    #endif

    int data_size = packet->header.length;

    // Send ACK after receiving only if received packet was not ACK
    if (packet->header.flags.ack != 1){
        rudp_send_ack(sock,client_addr,packet->header.seq_ack_number+1);
    }
    else {
        ack_received++;
    }
    
    return data_size;
}

// return the sequence number received
int rudp_recv_syn(int sock, struct sockaddr_in *client_addr){
    rudp_packet* packet = NULL;

    packet = (rudp_packet*) malloc (sizeof(rudp_packet));
    if (packet == NULL){
        fprintf(stderr, "MEMORY ALLOCATION ERROR!");
        close(sock);
        exit(FAIL);
    }

    // keep receiving packets until receiving a packet flagged as SYN
    do {
        rudp_recv_packet(sock, packet, sizeof (*packet), client_addr);
        if (packet == NULL){
            return -1;        // FAIL
        }
    } while (packet->header.flags.syn != 1 );

    int seq = packet->header.seq_ack_number;
    // Received and sent ack if needed -> free packet
    free(packet);

    return seq;
}

// return the sequence number received
int rudp_recv_ack(int sock, struct sockaddr_in *sender_addr){
    rudp_packet* packet = NULL;

    packet = (rudp_packet*) malloc (sizeof(rudp_packet));
    if (packet == NULL){
        fprintf(stderr, "MEMORY ALLOCATION ERROR!");
        close(sock);
        return -1;
    }

    // keep receiving packets until receiving a packet flagged as ACK
    do {
        rudp_recv_packet(sock, packet, sizeof (*packet), sender_addr);
        if (packet == NULL){
            return -1;        // FAIL
        }
    } while (packet->header.flags.ack != 1);

    int seq = packet->header.seq_ack_number;
    // Received and sent ack if needed -> free packet
    free(packet);

    return seq;
}

char* get_packet_type(rudp_packet* packet){
    char* type;
    if (packet->header.flags.ack == 1 && packet->header.flags.syn == 1)
        type = "SYN ACK ";
    else if (packet->header.flags.ack == 1)
        type = "ACK ";
    else if (packet->header.flags.syn == 1)
        type = "SYN ";
    else
        type = "";
    return type;
}

/*
* @brief A checksum function that returns 16 bit checksum for data.
* @param data The data to do the checksum for.
* @param bytes The length of the data in bytes.
* @return The checksum itself as 16 bit unsigned number.
* @note This function is taken from RFC1071, can be found here:
* @note https://tools.ietf.org/html/rfc1071
* @note It is the simplest way to calculate a checksum and is not very strong.
* However, it is good enough for this assignment.
* @note You are free to use any other checksum function as well.
* You can also use this function as such without any change.
*/
unsigned short int calculate_checksum(void *data, unsigned int bytes) {
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
    // Main summing loop
    while (bytes > 1) {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
    // Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);
    // Fold 32-bit sum to 16 bits
    while (total_sum >> 16){
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    }
    return (~((unsigned short int)total_sum));
}

// A simple calculating for packet loss - not 100% accurate but gives a reasonable result to improve perfomance 
float calculate_packet_loss() {
    if (packets_sent == 0) {
        return 0.0; // no packets sent. 0 loss
    }
    return ((1.0 - ((float)ack_received / (float)packets_sent)) * 100.0)/2;
}

// tries to deal with the packet loss by adjusting the timeout and retries for best performance
/* From my tests I found that the recommended values for best performance are:
(Big loss: the timeout should be as low as possible and max_retries as big as possible)
 * 0% loss: 5 retries, 1 timeout_sec
 * 2% loss: 100 retries, 1000 timeout_usec
 * 5% loss: 400 retries, 100 timeout_usec
 * 10% loss: 500 retries, 15 timeout_usec
 * 50% loss: 1000 retries, 15 timeout_usec
 * 75% loss: 10000 retries, 15 timeout_usec
*/
float loss_optimization(){
    // this is not the best but It does do a great job dealing with high losses
    float packet_loss = calculate_packet_loss();

    if (packet_loss < 1) {
        MAX_RETRIES = 100;
        TIMEOUT_SEC = 1;
        TIMEOUT_USEC = 0;
    } else if (packet_loss < 2) {
        MAX_RETRIES = 100;
        TIMEOUT_SEC = 0;
        TIMEOUT_USEC = 1000;
    } else if (packet_loss < 6) {
        MAX_RETRIES = 100;
        TIMEOUT_SEC = 0;
        TIMEOUT_USEC = 500;
    } else if (packet_loss < 11) {
        MAX_RETRIES = 400;
        TIMEOUT_SEC = 0;
        TIMEOUT_USEC = 100;
    } else if (packet_loss < 43) {
        MAX_RETRIES = 500;
        TIMEOUT_SEC = 0;
        TIMEOUT_USEC = 15;
    } else if (packet_loss < 53) {
        MAX_RETRIES = 5000;
        TIMEOUT_SEC = 0;
        TIMEOUT_USEC = 15;
    } else {
        MAX_RETRIES = 10000;
        TIMEOUT_SEC = 0;
        TIMEOUT_USEC = 0;
    }

    return packet_loss;
}