#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define _DEBUG

#define EXIT_MESSAGE "EXIT"

#define SERVER_PORT 6000
#define SERVER_IP "127.0.0.1"
#define MIN_FILE_SIZE 2097152           // 2MB

char* util_generate_random_data(unsigned int size);

int main(int argc, char *argv[]){
    printf("Starting Sender...\n");

    // if (argc != 7){
    //     fprintf(stderr, "Usage: -ip <server_ip> -p <server_port> -algo <algo>");
    //     exit(1);
    // }

    int sock = -1;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sock < 0){
        perror("sock");
        exit(1);
    }

    struct sockaddr_in server;

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &(server.sin_addr)) <= 0){
        perror("inet_pton");
        close(sock);
        exit(1);
    }


    // int i = 0;
    // Take port and ip from argv (if failed - use defaults)
    // while (i < argc){
    //     if (argv[i][0] == '-') {
    //         if (!strcmp(argv[i], "-algo")){
    //             i++;
    //             if (!strcmp(argv[i], "reno")){

    //             }
    //             else if (!strcmp(argv[i], "cubic")){

    //             }
    //             else {
    //                 fprintf(stderr, "Algo can be either reno for TCP Reno or cubic for TCP Cubic");
    //                 exit(1);
    //             }
    //         }
    //         else if (!strcmp(argv[i], "-p")){
    //             i++;
    //             server.sin_port = htons(*argv[i]);
    //         }
    //         else if (!strcmp(argv[i], "-ip")){
    //             i++;
    //             if (inet_pton(AF_INET, SERVER_IP, &(server.sin_addr)) <= 0){
    //                 perror("inet_pton");
    //                 exit(1);
    //             }
    //         }
    //     }
    //     i++;
    // }
    

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1){
        perror("connect");
        close(sock);
        exit(1);
    }

    // Generate random data
    printf("Generating random data, of at least %dMB in size...\n", MIN_FILE_SIZE);
    char *data = util_generate_random_data(MIN_FILE_SIZE+BUFSIZ);       // Generate data bigger than 2MB

    // FILE * f = fopen ("file.txt", "rb");
    // data = NULL;
    // size_t len;
    // ssize_t bytes_read = getdelim( &data, &len, '\0', f);
    #ifdef _DEBUG
    printf("Data generated: %s", data);
    #endif

    int bytes_sent, total_bytes_sent;
    unsigned short action = 1;

    while (action == 1) {
        printf("Sending the data...\n");
        bytes_sent = total_bytes_sent = 0;
        // TODO: understand how to resend bytes that went missing
        do {
            bytes_sent = send(sock, (data+total_bytes_sent), strlen(data)+1-total_bytes_sent, 0);
            // bytes_sent = send(sock, data, strlen(data)+1, 0);
            if (bytes_sent == -1){
                perror("send");
                close(sock);
                exit(1);
            }
            else if (bytes_sent == 0){
                printf("Connection was closed prior to sending the data!\n");
                close(sock);
                exit(1);
            }
            total_bytes_sent += bytes_sent;
        } while (total_bytes_sent < strlen(data)+1);

        printf("Do you want to send the file again?\n");
        printf("0 - No\n1 - Yes\n");
        scanf("%hu", &action);
    }

    printf("Sending an exit message to the receiver...\n");
    bytes_sent = send(sock, EXIT_MESSAGE, strlen(EXIT_MESSAGE)+1, 0);
    if (bytes_sent == -1){
        perror("send");
        close(sock);
        exit(1);
    }
    else if (bytes_sent == 0){
        printf("Connection was closed prior to sending the data!\n");
        close(sock);
        exit(1);
    }

    sleep(1);
    
    printf("Closing the TCP connection...\n");
    close(sock);

    printf("Sender end.");

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