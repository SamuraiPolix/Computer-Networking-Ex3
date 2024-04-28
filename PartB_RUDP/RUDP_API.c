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

/*
 * Declating Functions:
*/
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
                    exit(FAIL);
                }
                printf("Error is fixed.\nPlease restart the program!\n");
                exit(FAIL);
            }
            else {
                perror("bind");
                exit(FAIL);
            }
        }
    }

    // Handshake
    printf("Handshake Started.\n");
    if (peer_type == CLIENT) {
        // send SYN to server
        rudp_send_syn(sock, server_address, *seq_number);
    } else if (peer_type == SERVER) {
        struct sockaddr_in client_addr;
        // wait for SYN from client
        *seq_number = rudp_recv_syn(sock, &client_addr);
    }
    *seq_number += 1;;
    printf("Handshake completed!\n");
    return sock;
}

int rudp_send(int sock_id, void *data, size_t data_size, int flags, struct sockaddr_in *to, uint16_t* seq_number)
{
    int chunk_size, remaining_bytes;
    int total_bytes_sent = 0, bytes_sent;
    while (total_bytes_sent < data_size){
        // calculate chunk size
        remaining_bytes = data_size-total_bytes_sent;
        // spliting large size data into chunks that fit the maximum allowed data size for RUDP
        chunk_size = remaining_bytes < RUDP_MAX_DATA_SIZE ? remaining_bytes : RUDP_MAX_DATA_SIZE-1; // -1 for '\0'
        
        // create an RUDP simple packet (with current data chunk - total_bytes_sent acts as a pointer)
        rudp_packet* packet = create_packet(data+total_bytes_sent, chunk_size, *seq_number);

        // send chunk
        bytes_sent = rudp_send_packet(packet, sock_id, to);     // actual bytes - data witout header
        
        total_bytes_sent += bytes_sent;
        *seq_number += 1;
    }

    return total_bytes_sent;
}

int rudp_recv(int sock, void * data, size_t data_size, struct sockaddr_in *client_addr){
    rudp_packet* packet = (rudp_packet *) malloc (sizeof(rudp_packet));

    if (packet == NULL){
        fprintf(stderr, "ERROR: Failed to allocate memory for the packet!\n");
        return 0;
    }
    socklen_t len = sizeof(struct sockaddr_in);
    int bytes = recvfrom(sock, packet, sizeof(*packet), 0, (struct sockaddr *) client_addr, &len);

    if (bytes <= -1){
        perror("recv");
        close(sock);
        exit(1);
    }
    else if (bytes == 0){
        printf("Connection was closed prior to receiving the data4!\n");
        close(sock);
        exit(1);
    }

    if (packet->header.length > data_size){
        fprintf(stderr, " ERROR!\n");
        // TODO: deal with error
    }

    data_size = packet->header.length;

    if (data != NULL){
        memcpy(data, packet->data, data_size);
        memcpy(data+data_size-1, "\0", 1);       // '\0' at the end of the data
    }

    #ifdef _DEBUG
    char* type = get_packet_type(packet);
    printf("Received %spacket, SEQ: %d\n", type, packet->header.seq_ack_number);
    #endif

    // Send ACK after receiving only if received packet was not ACK
    if (packet->header.flags.ack != 1){
        rudp_send_ack(sock, client_addr, packet->header.seq_ack_number+1);
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
int rudp_send_packet(rudp_packet* packet, int sock_id, struct sockaddr_in *to){
    int bytes_sent;
    int tried = 0;      // count num of tries to get an ack packet back
    // send packet
    do {    // while ack not received or timedout
        bytes_sent = sendto(sock_id, packet, sizeof(*packet), 0, (struct sockaddr *) to, sizeof(*to));
        if (bytes_sent == -1){
            perror("sendto");
            close(sock_id);
            exit(FAIL);
        }
        else if (bytes_sent == 0){
            printf("Connection was closed prior to sending the data3!\n");
            close(sock_id);
            exit(FAIL);
        }
        bytes_sent = packet->header.length;     // actual data size

        #ifdef _DEBUG
        char* type = get_packet_type(packet);
        printf("Sending %spacket, SEQ: %d\n", type, packet->header.seq_ack_number);
        #endif

        // wait for ack
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = TIMEOUT_USEC;

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_id, &read_fds);

        

        

    } while (packet->header.flags.ack != 1 && rudp_recv_ack(sock_id, to) == -1);     // Resend if ack was not received. only if this packet is not an ack

    // 1. ACK was received or timedout OR 2. this is an ACK -> free packet
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

    packet->header.length = data_size;
    packet->header.seq_ack_number = seq_ack_number;

    // copy data
    memcpy(packet->data, data, data_size);
    memcpy(packet->data+data_size-1, "\0", 1);       // '\0'
    return packet;
}

int rudp_recv_packet(int sock, rudp_packet * packet, size_t packet_size, struct sockaddr_in *client_addr){
    socklen_t len = sizeof(struct sockaddr_in);
    recvfrom(sock, packet, sizeof(*packet), 0, (struct sockaddr *) client_addr, &len);

    #ifdef _DEBUG
    char* type = get_packet_type(packet);
    printf("Received %spacket, SEQ: %d\n", type, packet->header.seq_ack_number);
    #endif
    // TODO func to get packet type

    int data_size = strlen(packet->data);

    // Send ACK after receiving only if received packet was not ACK
    if (packet->header.flags.ack != 1){
        rudp_send_ack(sock,client_addr,packet->header.seq_ack_number+1);
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
    // TODO add timeout?

    // Received and sent ack if needed -> free packet
    free(packet);

    return packet->header.seq_ack_number;
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
    // TODO add timeout?

    // Received and sent ack if needed -> free packet
    free(packet);

    return packet->header.seq_ack_number;
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