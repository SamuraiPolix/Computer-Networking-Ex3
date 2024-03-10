#include "RUDP_API.h"
#include <stdio.h>
#include <string.h>

#define USAGE "-ip <server_ip> -p <server_port>"


int main(int argc, char *argv[]){
    if (argc != 5){
        fprintf(stderr, "Usage: %s", USAGE);
        exit(1);
    }

    struct sockaddr_in server;

    // Getting info from main's args into ip and port of server
    for (int i = 0; i < argc; i += 2){
        if (strcmp(argv[i], "-ip") == 0){
            if (inet_pton(AF_INET, argv[i+1], &(server.sin_addr)) <= 0){
                perror("inet_pton");
                exit(1);
            }
        }
        else if (strcmp(argv[i], "-p") == 0){
            server.sin_port = htons(argv[i+1]);
        }
        else{
            fprintf(stderr, "Incorrect argument! Usage: %s", USAGE);
            exit(1);
        }
    }

    int sock = rudp_socket((struct sockaddr_in*) &server, CLIENT);

    
}