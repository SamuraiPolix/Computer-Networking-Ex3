#include "RUDP_API.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

/*
 * Defines:
*/
#define MIN_FILE_SIZE 2*MB           // 2MB
#define USAGE "-ip <server_ip> -p <server_port>"
extern int *b; /* only declaration, b is defined in other file.*/

/*
 * Declaring Functions:
*/
char* util_generate_random_data(unsigned int size);

/*
 * Functions:
*/
int main(int argc, char *argv[]){
    uint16_t seq = 0;        // TODO randomize the first seq number
    printf("Starting Sender...\n");
    #ifndef _DEBUG
    if (argc != 5){
        fprintf(stderr, "Usage: %s", USAGE);
        exit(1);
    }
    #endif
    struct sockaddr_in server;

    // reset address's memory before using it
    memset(&server, 0, sizeof(server));
    #ifndef _DEBUG
    // Getting info from main's args into ip and port of server
    for (int i = 1; i < argc; i += 2){
        if (strcmp(argv[i], "-ip") == 0){
            // Set server ip
            if (inet_pton(AF_INET, argv[i+1], &(server.sin_addr)) <= 0){
                perror("inet_pton");
                exit(1);
            }
            #ifdef _DEBUG
            printf("Server IP is set to: %s\n", argv[i+1]);
            #endif
        }
        else if (strcmp(argv[i], "-p") == 0){
            // Set port
            server.sin_port = htons(atoi(argv[i+1]));
            #ifdef _DEBUG
            printf("Port is set to: %d\n", atoi(argv[i+1]));
            #endif
        }
        else{
            fprintf(stderr, "Incorrect argument! Usage: %s", USAGE);
            exit(1);
        }
    }
    #endif

    #ifdef _DEBUG
    server.sin_port = htons(2000);
    if (inet_pton(AF_INET, "127.0.0.1", &(server.sin_addr)) <= 0){
                perror("inet_pton");
                exit(1);
    }
    #endif
    // generate random num as a starting seq number
    srand(time(NULL));   // Initialization, should only be called once.
    // seq = rand();      // Returns a pseudo-random integer between 0 and RAND_MAX.
    seq = 0;

    int sock = rudp_socket((struct sockaddr_in*) &server, CLIENT, &seq);

    // Generate random data
    printf("Generating random data, of at least %dMB in size...\n", MIN_FILE_SIZE/MB);
    char *data = util_generate_random_data(MIN_FILE_SIZE+BUFSIZ);       // Generate data bigger than 2MB
    if (data == NULL){
        fprintf(stderr, "ERROR! Failed to allocate memory!\n");
        exit(FAIL);
    }

    int bytes_sent, total_bytes_sent;
    int file_size;
    char action;

    do {
        printf("Sending the data...\n");
        bytes_sent = total_bytes_sent = 0;

        // Send the size of the file so the receiver is prepared to receive all the bytes
        file_size = strlen(data)+1;
        bytes_sent = rudp_send(sock, &file_size, sizeof file_size, 0, &server, &seq);
        if (bytes_sent == -1){
            perror("send");
            rudp_close(sock);
            free(data);
            exit(1);
        }
        else if (bytes_sent == 0){
            printf("Connection was closed prior to sending the data!\n");
            rudp_close(sock);
            free(data);
            exit(FAIL);
        }

        // Send data
        bytes_sent = rudp_send(sock, data, file_size, 0, &server, &seq);
        if (bytes_sent == -1){
            perror("send");
            rudp_close(sock);
            free(data);
            exit(FAIL);
        }
        else if (bytes_sent == 0){
            printf("Connection was closed prior to sending the data!\n");
            rudp_close(sock);
            free(data);
            exit(FAIL);
        }

        
        #ifdef _DEBUG
        printf("Sent data size: %d bytes.\n", bytes_sent);
        #endif

        loss_optimization();

        printf("Do you want to send the file again?\n");
        printf("N - No\nY - Yes\n");
        // Get chars until one of the following: y,Y,n,N is received
        while (1){
            action = getchar();
            if (action != '\n'){
                if (action != 'y' && action != 'n' && action != 'Y' && action != 'N' )
                    printf("Incorrect selection! Enter:\nN - NO\nY - Yes\n");
                else
                    break;      // good input was received
            }
        }
        // Resend if received "yes"
    } while(action == 'y' || action == 'Y');

    total_bytes_sent = 0;
    printf("Sending an exit message to the receiver...\n");
    /*
    We notify the Receiver of an EXIT MESSAGE by "preparing him" to receive 0 bytes.
    */
    bytes_sent = rudp_send(sock, &total_bytes_sent, sizeof total_bytes_sent, 0, &server, &seq);     // telling him we have 0 bytes to send
    if (bytes_sent == -1){
        perror("send");
        rudp_close(sock);
        free(data);
        exit(FAIL);
    }
    else if (bytes_sent == 0){
        printf("Connection was closed prior to sending the data!\n");
        rudp_close(sock);
        free(data);
        exit(FAIL);
    }

    sleep(1);
    
    printf("Closing the RUDP connection...\n");
    rudp_close(sock);
    #ifdef _DEBUG
    printf("Max tries: %d\n", *b);
    printf("packet loss: %f\n", loss_optimization());
    #endif
    printf("Sender end.\n");
    free(data);

    return 0;
}


/*
 * @brief   A random data generator function based on srand() and rand().
 * @param   The size of the data to generate (up to 2^32 bytes).
 * @return  A pointer to the buffer.
 */
char* util_generate_random_data(unsigned int size){
    char *buffer = NULL;

    if (size == 0){
        return NULL;
    }

    buffer = (char*)malloc(size * sizeof(char));

    if (buffer == NULL){
        return NULL;
    }

    srand(time(NULL));

    for (unsigned int i = 0; i < size-1; i++){
        // set a random value insidee - make sure the value isn't '\0' that will act as the end of the string
        while ((*(buffer + i) = ((unsigned int) rand() % 256)) == '\0');
    }
    buffer[size-1] = '\0';

    return buffer;
}