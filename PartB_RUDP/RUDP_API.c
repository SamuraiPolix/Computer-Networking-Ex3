#include "RUDP_API.h"
#include <stdio.h>

#define RUDP_MAX_PACKET_SIZE 576        // Sources: RFC 791, RFC 1122, RFC 2460

#define ACK_WAIT_DELAY 2        // waiting time for ack to arrive when sending, in seconds

#define SUCCESS 0
#define FAIL 1


// Declare functions used here
// TODO

/*
 * This file contain all implementations for the RUDP API functions.
 */

/*
    0 1 2 3 4 5 6 7 8            15 16                            31 
   +-+-+-+-+-+-+-+-+---------------++-+-+-+-+-+-+-+-+---------------+
   |                               |                                |
   |             Source            |           Destination          |
   |              Port             |              Port              |
   |                               |                                |
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

// #define ACK_MASK 64
// #define CHK_MASK 4

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
    // uint16_t source_port;
    // uint16_t destination_port;
    uint16_t length;      // 2 bytes unassigned int - length of the data itself, without the RUDP header
    uint16_t checksum;      // 2 bytes unassigned int - used to validate the corectness of the data
    uint16_t seq_ack_number;    // when sending a packet - seq number is stored here. when sending an ack, the ack number is stored here.
    flags_bitfield flags;          // 1 byte unassigned int - used to classify the packet (SYN, ACK, etc.)

} rudp_packet_header;

typedef struct _rudp_packet {
    rudp_packet_header header;          // header, defined above
    char data[RUDP_MAX_PACKET_SIZE - sizeof(rudp_packet_header)];      // data without header
} rudp_packet;

static const int RUDP_MAX_DATA_SIZE = RUDP_MAX_PACKET_SIZE - sizeof(rudp_packet_header);

// Declaring functions
rudp_packet* create_ack_packet(int ack_number);
rudp_packet* create_syn_packet(int seq_number);
rudp_packet* create_ack_syn_packet(int seq_number);
int rudp_send_packet(rudp_packet* packet, int sock_id, struct sockaddr_in *to);
int rudp_send_ack(int sock_id, struct sockaddr_in *to, int seq_number);
int rudp_send_syn(int sock_id, struct sockaddr_in *to, int seq_number);
rudp_packet* create_packet(void *data, size_t data_size, int seq_ack_number);
int rudp_recv_syn(int sock, struct sockaddr_in *client_addr);
int rudp_recv_ack(int sock, struct sockaddr_in *sender_addr);

// Functions
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

    // handshake
    if (peer_type == CLIENT) {
        // send SYN to server
        printf("Sending SYN request (handshake)...\n");
        rudp_send_syn(sock, server_address, *seq_number);
        *seq_number += 1;
        // printf("Waiting for ACK...\n");
        // uint16_t seq = rudp_recv_ack(sock, &server_address);
        // printf("Received ACK.\n");
        printf("Handshake complete!\n");
    } else if (peer_type == SERVER) {
        struct sockaddr_in client_addr;
        // wait for SYN from client
        printf("Waiting for SYN request (handshake)...\n");
        *seq_number = rudp_recv_syn(sock, &client_addr);
        printf("Received SYN request (handshake).\n");
        *seq_number += 1;;
        // send ACK back to client
        // printf("Sending ACK...\n");
        // rudp_send_ack(sock, &client_addr, *seq_number);
        printf("Handshake complete!\n");

    }

    return sock;
}

int rudp_send(int sock_id, void *data, size_t data_size, int flags, struct sockaddr_in *to, uint16_t* seq_number)
{
    int chunk_size, remaining_bytes;
    int total_bytes_sent = 0, bytes_sent;
    while (total_bytes_sent < data_size){
        // calculate chunk size
        remaining_bytes = data_size-total_bytes_sent;
        chunk_size = remaining_bytes < RUDP_MAX_DATA_SIZE ? remaining_bytes : RUDP_MAX_DATA_SIZE;

        if (total_bytes_sent + 2000 > data_size)
            printf(".");
        
        // create rudp simple packet (with current data chunk - total_bytes_sent acts as a pointer)
        rudp_packet* packet = create_packet(data+total_bytes_sent, chunk_size, *seq_number);

        // send chunk
        bytes_sent = rudp_send_packet(packet, sock_id, to);
        if (bytes_sent == -1){
            perror("send");
            close(sock_id);
            free(data);
            return FAIL;
        }
        else if (bytes_sent == 0){
            printf("Connection was closed prior to sending the data!\n");
            close(sock_id);
            free(data);
            return FAIL;
        }
        total_bytes_sent += bytes_sent;
        *seq_number += 1;
    }

    return total_bytes_sent;
}

int rudp_send_packet(rudp_packet* packet, int sock_id, struct sockaddr_in *to){
    int bytes_sent;
    // send packet
    do {
        bytes_sent = sendto(sock_id, packet, sizeof(*packet), 0, (struct sockaddr *) to, sizeof(*to));
        if (bytes_sent == -1){
            perror("sendto");
            close(sock_id);
            exit(FAIL);
        }
        else if (bytes_sent == 0){
            printf("Connection was closed prior to sending the data!\n");
            close(sock_id);
            exit(FAIL);
        }
    } while (packet->header.flags.ack != 1 && rudp_recv_ack(sock_id, to) == FAIL);     // Resend if ack was not received. only if this packet is not an ack

    // 1. ACK was received or timedout OR 2. this is an ACK -> free packet
    free(packet);

    return bytes_sent;
}

int rudp_send_ack(int sock_id, struct sockaddr_in *to, int seq_number){
    // create rudp ack packet
    
    rudp_packet* ack_packet = create_ack_packet(seq_number);

    return rudp_send_packet(ack_packet, sock_id, to);
}

int rudp_send_syn(int sock_id, struct sockaddr_in *to, int seq_number){
    // create rudp ack packet
    
    rudp_packet* syn_packet = create_syn_packet(seq_number);

    return rudp_send_packet(syn_packet, sock_id, to);
}

rudp_packet* create_ack_packet(int ack_number){
    rudp_packet* ack_packet = create_packet(NULL, 0, ack_number);
    // set NUL flag to 1 -> set ACK flag to 1 - this is an ACK packet; following draft guidelines
    ack_packet->header.flags.nul = 1;
    ack_packet->header.flags.ack = 1;
    return ack_packet;
}

rudp_packet* create_syn_packet(int seq_number){
    rudp_packet* syn_packet = create_packet(NULL, 0, seq_number);
    // set NUL flag to 1 -> set SYN flag to 1 - this is a SYN packet; following draft guidelines
    syn_packet->header.flags.nul = 1;
    syn_packet->header.flags.syn = 1;
    return syn_packet;
}

rudp_packet* create_ack_syn_packet(int seq_number){
    rudp_packet* syn_packet = create_packet(NULL, 0, seq_number);
    syn_packet->header.flags.nul = 1;
    syn_packet->header.flags.syn = 1;
    syn_packet->header.flags.ack = 1;
    return syn_packet;
}

// Doesn't deal with flags - defaults to all 0
rudp_packet* create_packet(void *data, size_t data_size, int seq_ack_number){
    rudp_packet* packet = (rudp_packet*) malloc (sizeof(rudp_packet));

    if (packet == NULL){
        fprintf(stderr, "ERROR: Failed to allocate memory for the packet!\n");
        return NULL;
    }
    packet->header.seq_ack_number = seq_ack_number;

    // default flags - all 0
    memset(&packet->header.flags, 0, sizeof(packet->header.flags));

    memcpy(packet->data, data, data_size);
    return packet;
}

int rudp_recv(int sock, void * data, size_t data_size, struct sockaddr_in *client_addr){
    rudp_packet* packet = (rudp_packet *) malloc (sizeof(rudp_packet));

    if (packet == NULL){
        fprintf(stderr, "ERROR: Failed to allocate memory for the packet!\n");
        return 0;
    }
    socklen_t len = sizeof(struct sockaddr_in);
    // TODO is it *packet or packet inside sizeof
    int bytes = recvfrom(sock, packet, sizeof(*packet), 0, (struct sockaddr *) client_addr, &len);

    if (bytes <= -1){
        perror("recv");
        close(sock);
        exit(1);
    }
    else if (bytes == 0){
        printf("Connection was closed prior to receiving the data!\n");
        close(sock);
        exit(1);
    }

    if (data != NULL){
        memcpy(data, packet->data, data_size);
    }
    
    char* type;

    if (packet->header.flags.ack == 1 && packet->header.flags.syn == 1)
        type = "SYN ACK ";
    else if (packet->header.flags.ack == 1)
        type = "ACK ";
    else if (packet->header.flags.syn == 1)
        type = "SYN ";
    else
        type = "";
    printf("Received %spacket, SEQ: %d\n", type, packet->header.seq_ack_number);
    // Send ACK after receiving only if received packet was not ACK
    if (packet->header.flags.ack != 1){
        printf("Sending ACK packet, SEQ: %d\n", packet->header.seq_ack_number+1);
        rudp_send_ack(sock, client_addr, packet->header.seq_ack_number+1);
    }

    // Received and send ack -> free packet
    free(packet);
    return strlen(data);
}

int rudp_recv_packet(int sock, rudp_packet * packet, size_t packet_size, struct sockaddr_in *client_addr){
    socklen_t len = sizeof(struct sockaddr_in);
    recvfrom(sock, packet, sizeof(*packet), 0, (struct sockaddr *) client_addr, &len);
    
    char* type;

    if (packet->header.flags.ack == 1 && packet->header.flags.syn == 1)
        type = "SYN ACK ";
    else if (packet->header.flags.ack == 1)
        type = "ACK ";
    else if (packet->header.flags.syn == 1)
        type = "SYN ";
    else
        type = "";
    printf("Received %spacket, SEQ: %d\n", type, packet->header.seq_ack_number);
    // Send ACK after receiving only if received packet was not ACK
    if (packet->header.flags.ack != 1){
        printf("Sending ACK packet, SEQ: %d\n", packet->header.seq_ack_number+1);
        rudp_send_ack(sock,client_addr,packet->header.seq_ack_number+1);
    }

    // Received and send ack -> free packet
    free(packet);
    
    return strlen(packet->data);
}

// return the sequence number received
int rudp_recv_syn(int sock, struct sockaddr_in *client_addr){
    rudp_packet* packet = NULL;

    packet = (rudp_packet*) malloc (sizeof(rudp_packet));
    if (packet == NULL){
        fprintf(stderr, "MEMORY ALLOCATION ERROR!");
        close(sock);
        return FAIL;
    }

    // keep receiving packets until receing a packet flagged as SYN
    do {
        rudp_recv_packet(sock, packet, sizeof (*packet), client_addr);
        // if (packet == NULL){
        //     return FAIL;        // FAIL
        // }
    } while (packet->header.flags.syn != 1 );
    // TODO add timeout?

    return packet->header.seq_ack_number;
}

// return the sequence number received
int rudp_recv_ack(int sock, struct sockaddr_in *sender_addr){
    rudp_packet* packet = NULL;

    packet = (rudp_packet*) malloc (sizeof(rudp_packet));
    if (packet == NULL){
        fprintf(stderr, "MEMORY ALLOCATION ERROR!");
        close(sock);
        return FAIL;
    }

    // keep receiving packets until receing a packet flagged as SYN
    do {
        rudp_recv_packet(sock, packet, sizeof (*packet), sender_addr);
        // if (packet == NULL){
        //     return FAIL;        // FAIL
        // }
    } while (packet->header.flags.ack != 1);
    // TODO add timeout?

    return packet->header.seq_ack_number;
}

int rudp_close(){

}