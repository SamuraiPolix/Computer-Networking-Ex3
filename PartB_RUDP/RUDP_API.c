#include "RUDP_API.h"
#include <stdio.h>

#define RUDP_MAX_PACKET_SIZE 576        // Sources: RFC 791, RFC 1122, RFC 2460

#define ACK_WAIT_DELAY 2        // waiting time for ack to arrive when sending, in seconds


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
   |S|A|E|R|N|C|T| |
   |Y|C|A|S|U|H|C|0|
   |N|K|K|T|L|K|S| |
   |  Sequence #   +   Ack Number  |
   +---------------+---------------+
   |            Checksum           |
   +---------------+---------------+
*/
typedef struct _rudp_packet_header {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;      // 2 bytes unassigned int - length of the data itself, without the RUDP header
    uint16_t checksum;      // 2 bytes unassigned int - used to validate the corectness of the data

    uint8_t flags;          // 1 byte unassigned int - used to classify the packet (SYN, ACK, etc.)

} rudp_packet_header;

typedef struct _rudp_packet {
    rudp_packet_header header;          // header, defined above
    char data[RUDP_MAX_PACKET_SIZE - sizeof(rudp_packet_header)];      // data without header
} rudp_packet;


int rudp_socket(struct sockaddr_in *my_addr, int peer_type)
{
    int sock = -1;

    // create a socket over UDP
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock == -1){
        perror("sock");
        exit(1);
    }

    // if peer_type is SERVER - this is a server that needs binding
    if (peer_type == SERVER) {
        if (bind(sock, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1){
            if (errno == EADDRINUSE) {
                // Deal with "Address already in use" error
                fprintf(stderr, "bind: Address already in use\n");
                printf("Fixing error...\n");
                int yes = 1;
                if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1){
                    fprintf(stderr, "Couldn't fix the error, Exiting...\n");
                    perror("setsockopt");
                    exit(1);
                }
                printf("Error is fixed.\nPlease restart the program!\n");
                exit(1);
            }
            else {
                perror("bind");
                exit(1);
            }
        }
    }

    return sock;
}

int rudp_send(int sock_id, const void *data, size_t data_size, struct sockaddr_in *to, uint8_t flags)
{
    int ack = 0;        // 1 if ack was received after sending, 0 if not
    // create rudp packet
    rudp_packet* packet = create_packet(data, data_size, flags);

    int bytes_sent;
    // send packet
    
    do {
        bytes_sent = sendto(sock_id, packet, sizeof(packet), 0, (struct sockaddr *)&to, sizeof(to));
        if (bytes_sent == -1){
            perror("sendto");
            close(sock_id);
            exit(1);
        }
        else if (bytes_sent == 0){
            printf("Connection was closed prior to sending the data!\n");
            close(sock_id);
            exit(1);
        }

        // wait for ACK response
        sleep(ACK_WAIT_DELAY);
        rudp_recv_ack();        // TODO

    } while (ack == 0);     // Resend if ack was not received
    


    return 0;
}

rudp_packet* create_packet(const void *data, size_t data_size, uint8_t flags){

}

int rudp_recv(){


}

int rudp_close(){

}