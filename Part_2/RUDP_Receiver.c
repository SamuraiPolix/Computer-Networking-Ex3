#include "RUDP_API.h"
#include <stdio.h>

#define USAGE "-p <server_port>"

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "Usage: %s", USAGE);
        exit(1);
    }

    struct sockaddr_in server, client;

    // Getting info from main's args into ip and port of server
    for (int i = 0; i < argc; i += 2){
        if (strcmp(argv[i], "-p") == 0){
            server.sin_port = htons(argv[i+1]);
        }
        else{
            fprintf(stderr, "Incorrect argument! Usage: %s", USAGE);
            exit(1);
        }
    }

    int sock = rudp_socket((struct sockaddr_in*) &server, SERVER);

    
}