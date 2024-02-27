#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define _DEBUG

#define EXIT_MESSAGE "EXIT"

#define SERVER_PORT 6000        // The port the server is listening on
#define CLIENTS 1           // Max allowed clients at once
#define MAX_RUNS 100000
#define MB 1048576
struct
{
    unsigned int id;            // Run number #_
    float elapsed_time;         // time it took to receive data
    float speed;           // speed in MB/s
} runs[MAX_RUNS];           // runs[0] keeps the avg of all runs


int main(int argc, char *argv[]) {
    printf("Starting Receiver...\n");
    int sock = -1;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0){
        perror("sock");
        exit(1);
    }

    struct sockaddr_in server, client;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = INADDR_ANY;        // accept connections from any ip
    
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
                exit(1);
            }
            printf("Error is fixed.\nPlease restart the program!\n");
        }
        else{
            perror("bind");
        }
        exit(1);
    }

    if (listen(sock, CLIENTS) == -1){
        perror("listen");
        close(sock);
        exit(1);
    }

    printf("Waiting for TCP connection...\n");

    int client_len = sizeof(client);

    int sock_client = accept(sock, (struct sockaddr *)&client, (socklen_t*)&client_len);
    if (sock_client == -1) {
        perror("accept");
        close(sock);
        exit(1);
    }

    printf("Sender connected, beginning to receive the file...\n");
    int times = 0;       // Save the amount of times data is received
    // ASK: when do I need to start the time calculation?
    struct timeval start_time, end_time;

    int bytes_received, total_bytes;
    // int total_bytes_received = 0;
    char buffer[BUFSIZ] = {0};

    do {
        times++;
        // TODO: fix times
        gettimeofday(&start_time, NULL);        // Log current time, to calculate time later

        // TODO: understand how to receive bytes that went missing. keep receiving if there was no '\0' at the end of the message?
        do {
            bytes_received = recv(sock_client, buffer, BUFSIZ-1, 0);
            if (strcmp())
            // total_bytes_received += bytes_received;
            if (bytes_received == -1){
                perror("recv");
                close(sock);
                exit(1);
            }
            else if (bytes_received == 0){
                printf("Connection was closed prior to receiving the data!\n");
                close(sock);
                exit(1);
            }
        } while (bytes_received > 0);

        // Check if the received message is an exit message
        if (!strcmp(buffer, EXIT_MESSAGE)) {
            printf("Sender sent exit message.\n");
            times--;        // don't count this message in the stats
            break;      // stop receiving
        }

        // Makes sure a '\0' exists at the end of the data to not accidently access forbidden memory
        if (buffer[BUFSIZ - 1] != '\0'){
            buffer[BUFSIZ - 1] = '\0';
        }

        // SAVE TO FILE??

        gettimeofday(&end_time, NULL);

        runs[times].id = times;
        runs[times].elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        runs[times].speed = (strlen(buffer)/(float)MB)/(runs[times].elapsed_time/1000.0);

        // runs[0] keeps the avg of all runs. add data to avg:
        runs[0].elapsed_time += runs[times].elapsed_time;
        runs[0].speed += runs[times].speed;

        #ifdef _DEBUG
        printf("Received data size: %ld: %s\n", strlen(buffer), buffer);
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

    for (int i = 1; i < times; i++){
        printf("Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", runs[i].id, runs[i].elapsed_time, runs[i].speed);
    }

    // divide to get avg
    runs[0].elapsed_time /= times;
    runs[0].speed /= times;

    printf("Average time: %.2fms\n", runs[0].elapsed_time);
    printf("Average bandwidth: %.2fMB/s\n", runs[0].speed);
    printf("----------------------------\n");
    
    printf("Receiver end.\n");
    return 0;
}