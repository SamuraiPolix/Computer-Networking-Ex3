#include "RUDP_API.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

/*
 * Defines:
*/
#define USAGE "-p <server_port>"
#define MAX_RUNS 10000
/*
 * Strcuts:
*/
struct
{
    unsigned int id;            // Run number #X
    float elapsed_time;         // time it took to receive data
    float speed;           // speed in MB/s
} runs[MAX_RUNS];           // runs[0] keeps the avg of all runs

/*
 * Functions:
*/
int main(int argc, char *argv[]){
    printf("Starting Receiver...\n");
    struct sockaddr_in server, client;
    // reset address's memory before using it
    memset(&server, 0, sizeof(server));
    #ifndef _DEBUG
    if (argc != 3){
        fprintf(stderr, "Usage: %s", USAGE);
        exit(FAIL);
    }

    // Getting info from main's args into ip and port of server
    for (int i = 1; i < argc; i += 2){
        if (strcmp(argv[i], "-p") == 0){
            // Set port
            server.sin_port = htons(atoi(argv[i+1]));
            #ifdef _DEBUG
            printf("Port is set to: %d\n", atoi(argv[i+1]));
            #endif
        }
        else{
            fprintf(stderr, "Incorrect argument! Usage: %s", USAGE);
            exit(FAIL);
        }
    }
    #endif
    #ifdef _DEBUG
    server.sin_port = htons(2000);
    #endif


    server.sin_family = AF_INET;        // ipv4
    server.sin_addr.s_addr = INADDR_ANY;        // accept connections from any ip

    // generate random num as a starting seq number
    srand(time(NULL));
    uint16_t seq = rand();

    int sock = rudp_socket((struct sockaddr_in*) &server, SERVER, &seq);

    printf("Sender connected, beginning to receive the file...\n");
    int times = 0;       // Save the amount of times data is received
    struct timeval start_time, end_time;

    int bytes_received, remaining_bytes, total_bytes;
    char buffer[BUFSIZ] = {0};

    do {
        times++;

        // Receive the size of the file in bytes (Sender prepares us for the file)
        bytes_received = rudp_recv(sock, &remaining_bytes, sizeof remaining_bytes, &client, &seq);

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
            bytes_received = rudp_recv(sock, buffer, BUFSIZ, &client, &seq);
            remaining_bytes -= bytes_received;
            #ifdef _DEBUG
            printf("Bytes received: %d\n", bytes_received);
            printf("Remaining received: %d\n", remaining_bytes);
            #endif
            if (bytes_received <= -1){
                perror("recv");
                rudp_close(sock);
                exit(FAIL);
            }
            else if (bytes_received == 0){
                printf("Connection was closed prior to receiving the data1!\n");
                rudp_close(sock);
                exit(FAIL);
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
    rudp_close(sock);

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