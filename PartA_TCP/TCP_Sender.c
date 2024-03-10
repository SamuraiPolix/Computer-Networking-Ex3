#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h> 
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define _DEBUG

#define EXIT_MESSAGE 0      // Exit message is sending 0 as of "we have 0 bytes to send"

// #define SERVER_PORT 6000
// #define SERVER_IP "127.0.0.1"

#define MB 1048576
#define MIN_FILE_SIZE 2*MB           // 2MB


char* util_generate_random_data(unsigned int size);

int main(int argc, char *argv[]){
    printf("Starting Sender...\n");

    // Check the correct amount of args were received
    if (argc != 7){
        fprintf(stderr, "Usage: -ip <server_ip> -p <server_port> -algo <algo>");
        exit(1);
    }

    // create a socket, ipv4, tcp type, tcp protocol (chosen automatically)
    int sock = -1;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sock < 0){
        perror("sock");
        exit(1);
    }

    struct sockaddr_in server;

    // reset address's memory before using it
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;        // ipv4

    int i = 1;
    // Take port, ip and algo from argv (if failed - stop program and ask for correct inputs)
    while (i < argc){
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-algo")){
                // Set TCP congestion control algorithm
                char *algo;       // "reno" or "cubic"
                i++;
                if (!strcmp(argv[i], "reno")){
                    algo = "reno";
                }
                else if (!strcmp(argv[i], "cubic")){
                    algo = "cubic";
                }
                else {
                    fprintf(stderr, "Algo should be \"reno\" or \"cubic\"!");
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
                // Set server port
                i++;
                server.sin_port = htons(atoi(argv[i]));
                #ifdef _DEBUG
                printf("Server Port is set to: %d\n", atoi(argv[i]));
                #endif
            }
            else if (!strcmp(argv[i], "-ip")){
                // Set server ip
                i++;
                if (inet_pton(AF_INET, argv[i], &(server.sin_addr)) <= 0){
                    perror("inet_pton");
                    exit(1);
                }
                #ifdef _DEBUG
                printf("Server IP is set to: %s\n", argv[i]);
                #endif
            }
        }
        i++;
    }

    // connect to server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1){
        perror("connect");
        close(sock);
        exit(1);
    }

    // Generate random data
    printf("Generating random data, of at least %dMB in size...\n", MIN_FILE_SIZE/MB);
    char *data = util_generate_random_data(MIN_FILE_SIZE+BUFSIZ);       // Generate data bigger than 2MB

    /*  USE A FILE WITH INCREASING NUMBER TO MAKE SURE THE DATA IS FULLY RECEIVED
    // OPEN FILE
    const char* file_path = "numbers.txt";
    FILE* file = fopen(file_path, "rb");
    
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // get size of file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // allocate memory for file
    data = (char*)malloc(file_size + 1);

    if (data == NULL) {
        perror("Memory allocation error");
        fclose(file);
        return 1;
    }

    // read file to data
    fread(data, 1, file_size, file);

    // add '\0' at the end to make sure we dont get to forbidden memory
    data[file_size] = '\0';

    // close file
    fclose(file);
    */


    // #ifdef _DEBUG
    // printf("Data generated: %s", data);
    // #endif

    int bytes_sent, total_bytes_sent;
    int packet_size;
    char action;

    do {
        printf("Sending the data...\n");
        bytes_sent = total_bytes_sent = 0;

        // Send the size of the file so the receiver is prepared to receive all the bytes
        packet_size = strlen(data)+1;
        bytes_sent = send(sock, &packet_size, sizeof packet_size, 0);
        if (bytes_sent == -1){
            perror("send");
            close(sock);
            free(data);
            exit(1);
        }
        else if (bytes_sent == 0){
            printf("Connection was closed prior to sending the data!\n");
            close(sock);
            free(data);
            exit(1);
        }

        // Send data
        bytes_sent = send(sock, data, strlen(data)+1, 0);
        if (bytes_sent == -1){
            perror("send");
            close(sock);
            free(data);
            exit(1);
        }
        else if (bytes_sent == 0){
            printf("Connection was closed prior to sending the data!\n");
            close(sock);
            free(data);
            exit(1);
        }

        #ifdef _DEBUG
        printf("Sent data size: %d bytes.\n", bytes_sent);
        #endif

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
    bytes_sent = send(sock, &total_bytes_sent, sizeof total_bytes_sent, 0);     // telling him we have 0 bytes to send
    if (bytes_sent == -1){
        perror("send");
        close(sock);
        free(data);
        exit(1);
    }
    else if (bytes_sent == 0){
        printf("Connection was closed prior to sending the data!\n");
        close(sock);
        free(data);
        exit(1);
    }

    sleep(1);
    
    printf("Closing the TCP connection...\n");
    close(sock);

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
