#include <arpa/inet.h>
#include <netinet/tcp.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define _DEBUG

#define EXIT_MESSAGE 0

// #define SERVER_PORT 6000        // The port the server is listening on
#define CLIENTS 1           // Max allowed clients in queue
#define MAX_RUNS 10000
#define MB 1048576

struct
{
    unsigned int id;            // Run number #X
    float elapsed_time;         // time it took to receive data
    float speed;           // speed in MB/s
} runs[MAX_RUNS];           // runs[0] keeps the avg of all runs

int main(int argc, char *argv[]) {
    printf("Starting Receiver...\n");

    // Check the correct amount of args were received
    if (argc != 5){
        fprintf(stderr, "Usage: -p <server_port> -algo <algo>");
        exit(1);
    }

    int sock = -1;
    // create a socket, ipv4, tcp type, tcp protocol (chosen automatically)
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0){
        perror("sock");
        exit(1);
    }

    struct sockaddr_in server, client;
    // reset address's memory before using it
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;        // ipv4
    server.sin_addr.s_addr = INADDR_ANY;        // accept connections from any ip

    int i = 1;
    // Take port, ip and algo from argv (if failed - stop program and ask for correct inputs)
    while (i < argc){
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-algo")){
                // Set TCP congestion control algorithm
                const char *algo;; // or "cubic"
                i++;
                if (!strcmp(argv[i], "reno")){
                    algo = "reno";
                }
                else if (!strcmp(argv[i], "cubic")){
                    algo = "cubic";
                }
                else {
                    fprintf(stderr, "Algo should be either reno for TCP Reno or cubic for TCP Cubic!\n");
                    exit(1);
                }
                // Set algo
                #ifdef _DEBUG
                printf("Algo is set to: %s\n", algo);
                #endif
                if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algo, strlen(algo)) < 0) {
                    perror("setsockopt");
                    close(sock);
                    exit(1);
                }
            }
            else if (!strcmp(argv[i], "-p")){
                // Set port
                i++;
                server.sin_port = htons(atoi(argv[i]));
                #ifdef _DEBUG
                printf("Port is set to: %d\n", atoi(argv[i]));
                #endif
            }
        }
        i++;
    }
    
    // bind address and port to the server's socket
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) == -1){
        close(sock);
        if (errno == EADDRINUSE) {
            // Deal with "Address already in use" error
            fprintf(stderr, "bind: Address already in use\n");
            printf("Fixing error...\n");
            int yes = 1;
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1){
                fprintf(stderr, "Couldn't fix the error, Exiting...\n");
                perror("setsockopt");
                close(sock);
                exit(1);
            }
            printf("Error is fixed.\nPlease restart the program!\n");
        }
        else{
            perror("bind");
        }
        exit(1);
    }

    // listen for connections. allowing CLIENTS clients in queue
    if (listen(sock, CLIENTS) == -1){
        perror("listen");
        close(sock);
        exit(1);
    }

    printf("Waiting for TCP connection...\n");

    int client_len = sizeof(client);

    // accept connection and store client's socket id
    int sock_client = accept(sock, (struct sockaddr *)&client, (socklen_t*)&client_len);
    if (sock_client == -1) {
        perror("accept");
        close(sock);
        exit(1);
    }

    printf("Sender connected, beginning to receive the file...\n");
    int times = 0;       // Save the amount of times data is received
    struct timeval start_time, end_time;

    int bytes_received, remaining_bytes, total_bytes;
    char buffer[BUFSIZ] = {0};

    do {
        times++;
        
        // Receive the size of the file in bytes (Sender prepares us for the file)
        bytes_received = recv(sock_client, &remaining_bytes, sizeof remaining_bytes, 0);

        // Check if the received message is an exit message
        if (remaining_bytes == EXIT_MESSAGE){
            printf("Sender sent exit message.\n");
            times--;        // don't count this message in the stats
            break;      // stop receiving
        }

        total_bytes = remaining_bytes;
        gettimeofday(&start_time, NULL);        // Log current time, to calculate time later

        // Receive the file
        do {
            bytes_received = recv(sock_client, buffer, BUFSIZ, 0);
            remaining_bytes -= bytes_received;
            if (bytes_received <= -1){
                perror("recv");
                close(sock);
                exit(1);
            }
            else if (bytes_received == 0){
                printf("Connection was closed prior to receiving the data!\n");
                close(sock);
                exit(1);
            }
            // Makes sure a '\0' exists at the end of the data to not accidently access forbidden memory - if we print the buffer
            // if (buffer[BUFSIZ - 1] != '\0'){
            //     buffer[BUFSIZ - 1] = '\0';
            // }
            // printf("%s.\n", buffer);

            // keep receiving until the amount of expected bytes is reached
        } while (remaining_bytes > 0);

        gettimeofday(&end_time, NULL);      // log end time

        // Makes sure a '\0' exists at the end of the data to not accidently access forbidden memory - if we print the buffer
        if (buffer[BUFSIZ - 1] != '\0'){
            buffer[BUFSIZ - 1] = '\0';
        }

        // Add stats
        struct timeval elapsed;
        timersub(&end_time, &start_time, &elapsed);
        runs[times].id = times;
        runs[times].elapsed_time = elapsed.tv_sec * 1000.0 + elapsed.tv_usec / 1000.0;
        runs[times].speed = (total_bytes / (float)MB) / (runs[times].elapsed_time / 1000.0);

        // runs[0] keeps the avg of all runs. add data to avg:
        runs[0].elapsed_time += runs[times].elapsed_time;
        runs[0].speed += runs[times].speed;

        #ifdef _DEBUG
        printf("Received data size: %d bytes.\n", total_bytes);
        #endif

        printf("Data transfer completed.\n");

        printf("Waiting for sender's response...\n");

        // Reset buffer before more data is received
        memset(buffer, 0, BUFSIZ);
    } while (1);

    // Close connection
    close(sock);

    // PRINT STATS

    printf("----------------------------\n");
    printf("-      * Statistics *      -\n");

    for (int i = 1; i <= times; i++){
        printf("Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", runs[i].id, runs[i].elapsed_time, runs[i].speed);
    }

    // divide by num of runs to get avg
    runs[0].elapsed_time /= times;
    runs[0].speed /= times;

    printf("Average time: %.2fms\n", runs[0].elapsed_time);
    printf("Average bandwidth: %.2fMB/s\n", runs[0].speed);
    printf("----------------------------\n");
    
    printf("Receiver end.\n");
    
    return 0;
}