#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#define MAX_COMMAND_SIZE 4096
#define starting_port 10000
#define maximum_port  20000
#define server_port 8080

void removeNewlineFromEnd(char *str) { 
    str[strcspn(str, "\n")] = 0;
}

void man_page() {
    printf("\n");
    printf("**** CLIENT COMMANDS MANUAL ****\n");
    printf("\n");
    printf("Commands and their usage:\n");
    printf("--------------------------------------------\n");
    printf("  READ <path>              : Read the contents of the specified file.\n");
    printf("  WRITE <path> <TEXT>      : Write the given text to the specified file.\n");
    printf("  STREAM <path>            : Stream the contents of the specified file.\n");
    printf("  LS                       : List all files and directories.\n");
    printf("  CREATE <path>            : Create a new file or directory at the specified path.\n");
    printf("  DELETE <path>            : Delete the specified file or directory.\n");
    printf("  COPY <source> <destination> : Copy a file or directory to the destination path.\n");
    printf("  INFO <path>              : Display detailed information about the specified file.\n");
    printf("  MAN                      : Display this manual page.\n");
    printf("  EXIT                     : Terminate the client program.\n");
    printf("--------------------------------------------\n");
    printf("\n");
    printf("Use these commands carefully to interact with the system.\n");
    printf("For further help, refer to the user documentation.\n");
    printf("***************\n");
    printf("\n");
}
int create_socket() {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0))<0) {
        perror("Error: Unable to create socket");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}
int locate_free_port() {
    int sockfd;
    struct sockaddr_in addr;
    int no_of_port=starting_port;
    while(no_of_port< maximum_port){
        sockfd = create_socket();
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(no_of_port);
        addr.sin_addr.s_addr = INADDR_ANY;
        int bind_status = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
            if(bind_status >= 0) {
                // Successful binding
                close(sockfd);
                return no_of_port;
            }else if(bind_status < 0) {
                if (errno == EADDRINUSE) {
                    // If the port is busy, try the next one
                    no_of_port++;
                    close(sockfd);
                } else {
                    perror("Unexpected error during binding");
                    close(sockfd);
                }
            }
    }
    fprintf(stderr, "No free ports available between %d and %d\n", starting_port, maximum_port);
    return -1;   
}
int SS_connection(char* ip_ss,int port,int ss_port){
    int sockfd;
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(ss_port);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("Error: Unable to create socket");
    }
    int opt = 1;
    int sock_opt = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(sock_opt < 0){
        perror("Error: Unable to set socket options");
        close(sockfd);
    }
    int bind_status = bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr));
    if(bind_status < 0){
        perror("Error: Unable to bind socket");
        close(sockfd);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(port); // Port
    int inet_pton_status = inet_pton(AF_INET, ip_ss, &server_addr.sin_addr);
    // printf("ip_storage server %s\n",ip_ss);
    if (inet_pton_status <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    // Connecting to the storage Server
    int connection_status=connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (connection_status < 0) {
        printf("could not connect to the storage server\n");
        perror("Connection failed");
        close(sockfd);
    }
    return sockfd;

}


int main(){
    char ns_ip[100];  // Allocate sufficient space for the input
    printf("Enter the Naming server IP: ");
    if (fgets(ns_ip, sizeof(ns_ip), stdin) != NULL) {
        // Remove the newline character if present
        size_t len = strlen(ns_ip);
        if (len > 0 && ns_ip[len - 1] == '\n') {
            ns_ip[len - 1] = '\0';
        }
    }
    int ns_port;
    ns_port = locate_free_port();
    if(ns_port == -1){
        printf("Error: No availiable client port for communication with NS\n");
        exit(EXIT_FAILURE);
    }
    else{
        printf("port for communication between Client and NS is: %d\n", ns_port);
    }
    int ss_port;
    ss_port = locate_free_port();
    if(ss_port == -1){
        printf("Error: No availiable client port for communication with SS\n");
        exit(EXIT_FAILURE);
    }
    else{
        printf("port for communication between Client and SS is: %d\n", ss_port);
    }
    int sockfd;
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(ns_port);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("Error: Unable to create socket");
    }
    int opt = 1;
    int sock_opt = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(sock_opt < 0){
        perror("Error: Unable to set socket options");
        close(sockfd);
    }
    int bind_status = bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr));
    if(bind_status < 0){
        perror("Error: Unable to bind socket");
        close(sockfd);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, '0', sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(server_port); // Port

    int inet_pton_status = inet_pton(AF_INET, ns_ip, &server_addr.sin_addr);
    printf("%s\n",ns_ip);
    if (inet_pton_status <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    // Connecting to the Naming Server
    int connection_status=connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (connection_status < 0) {
        perror("Connection failed");
        close(sockfd);
    }
    printf("Connected to the Naming  server\n");

    printf("ALL CONNECTIONS ESTABLISHED SUCESSFULLY!!!\n");
    while (1)
    { 
        char command[MAX_COMMAND_SIZE];
        memset(command, 0, MAX_COMMAND_SIZE);
        printf("Enter a Command: ");
        fgets(command, MAX_COMMAND_SIZE, stdin);
        if(strlen(command) == 0){
            printf("Invalid command\n");
            continue;
        }
        if(strcmp(command,"\n")==0){
            printf("Invalid command\n");
            continue;
        }
        // removeNewlineFromEnd(command);
         command[strcspn(command, "\n")] = 0;
        if (strcmp(command, "EXIT") == 0) {
            // free(command);
            break;
        }
        if(strcmp(command,"MAN")==0){
            // free(command);
            man_page();
            continue;
        }
        char instruction[100];
        int num_tokens = sscanf(command, "%s", instruction);
        if (num_tokens == 0) {
            printf("Invalid command\n");
            continue;
        }
        int send_to_ns = send(sockfd, command, strlen(command), 0);
        if (send_to_ns < 0){
            perror("Failed sending request to NS");
            close(sockfd);
        }
        char response_from_NS[1024];
        memset(response_from_NS, 0, sizeof(response_from_NS)); // Clear the buffer

        int recv_from_ns = recv(sockfd, response_from_NS, sizeof(response_from_NS) - 1, 0); // Receiving data
        if (recv_from_ns < 0) {
            perror("Failed to receive data from Naming Server");
            exit(EXIT_FAILURE);
        }
        response_from_NS[recv_from_ns] = '\0'; // Ensure null termination
        printf("Length of response: %ld\n", strlen(response_from_NS));
        if(strcmp(command,"LS")!=0){
            printf("Response from Naming Server: %s\n", response_from_NS);
            printf("End of response\n");
        }
            // break;
        if (recv_from_ns < 0) {
            perror("Failed receiving response from NS");
            close(sockfd);
            // free(response_from_NS); 
            exit(EXIT_FAILURE);
        } else if (recv_from_ns == 0) {
            printf("Connection closed by naming server.\n");
            close(sockfd);
            // free(response_from_NS);
            exit(EXIT_SUCCESS);
        }
        if(strcmp(command,"LS")==0){
            //no need to request the ss , NS response will be the list of all the files/folders
            printf("RESPONSE FORM NS %s\n",response_from_NS);
            // free(response_from_NS);
            continue;
        }
        //connect to the storage server
        if(memcmp(response_from_NS,"lookup response",15)==0){
            //    printf("RESPONSE FORM NS %s\n",response_from_NS);
            //    printf("end of response\n");
                char *response = strtok(response_from_NS, "\n");  // Get the first token
                int count = 0;  // Counter to limit to 2 iterations
                char ip_ss[100];
                int port_ss;
                while (response != NULL && count < 2) {
                    response = strtok(NULL, "\n");  // Get the next token
                    if (memcmp(response, "ip:", 3) == 0) {
                        // Extract the IP address from the token
                        sscanf(response, "ip:%s", ip_ss);
                    } else if (memcmp(response, "nm_port:", 8) == 0) {
                        // Extract the port number from the token
                        sscanf(response, "nm_port:%d", &port_ss);
                    }
                    count++;  // Increment the counter
                }
                // free(response_from_NS);
                printf("ip_ss:%s ,port_ss: %d\n",ip_ss,port_ss);
                int ss_sock =SS_connection(ip_ss, port_ss, ss_port);
                if (ss_sock < 0) {
                    perror("Error: Unable to connect to the storage server");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                int send_to_ss = send(ss_sock, command, strlen(command), 0);
                if (send_to_ss < 0) {
                    perror("Failed sending request to SS");
                    close(ss_sock);
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                char response_from_SS[1024];
                memset(response_from_SS, 0, sizeof(response_from_SS)); // Clear the buffer
        
                printf("RESPONSE FORM SS %s\n",response_from_SS);
                // if (response_from_SS == NULL) {
                //     perror("Memory allocation failed");
                //     close(ss_sock);
                //     close(sockfd);
                //     exit(EXIT_FAILURE);
                // }
                if(memcmp(command,"READ",4)==0){
                        int bytes_received;
                        char buffer[1024];
                        while ((bytes_received = recv(ss_sock, buffer, sizeof(buffer), 0)) > 0) {
                            // Print the received chunk of data
                            fwrite(buffer, 1, bytes_received, stdout);
                        }
                        if (bytes_received == -1) {
                            perror("\nReceive failed\n");
                        } else {
                            printf("\nData received successfully\n");
                        }
                        close(ss_sock);
                        continue;
                }
                else if(memcmp(command,"STREAM",6)==0){
                     printf("Streaming audio data\n");

                     FILE *pipe = popen("mpg123 --verbose -", "w");;  // Note: We are using `-` to read from stdin
                        if (!pipe) {
                            perror("Failed to open mpg123");
                            
                            close(ss_sock);
                            return 1;
                        }
                    int bytes_received;
                    // char buffer[8192];
                    char* buffer =(char*)malloc(sizeof(char)*8192);
                    
                    // memset(buffer, 0, sizeof(buffer));
                    // Stream audio data directly to mpg123, ensuring we send larger chunks
                        while ((bytes_received = recv(ss_sock, buffer, 8192, 0)) > 0) {
                            fwrite(buffer, 1, bytes_received, pipe);  // Send the received data to mpg123
                            // printf("buffer: %s\n",buffer);
                        }

                        if (bytes_received < 0) {
                            perror("Error receiving data");
                        }

                        // Clean up
                        pclose(pipe);
                        close(ss_sock);

                         continue;
                }
                // else if(memcmp(command,"STREAM",6)==0){
                //         printf("Streaming audio data\n");

                //         // Open pipe to mpg123 with more robust playback
                //         FILE *pipe = popen("mpg123 -", "w");  // -q for quiet mode
                //         if (!pipe) {
                //             perror("Failed to open mpg123");
                //             close(ss_sock);
                //             return 1;
                //         }

                //         int bytes_received;
                //         char buffer[BUFSIZ];  // Use standard buffer size
                //         size_t total_bytes_received = 0;
                        
                //         // Set socket to blocking mode if not already
                //         int flags = fcntl(ss_sock, F_GETFL, 0);
                //         fcntl(ss_sock, F_SETFL, flags & ~O_NONBLOCK);

                //         // Implement more robust streaming
                //         while (1) {
                //             bytes_received = recv(ss_sock, buffer, sizeof(buffer), 0);
                            
                //             if (bytes_received <= 0) {
                //                 if (bytes_received == 0) {
                //                     printf("Connection closed. Total bytes received: %zu\n", total_bytes_received);
                //                     break;
                //                 }
                                
                //                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
                //                     // Temporary error, try again
                //                     usleep(10000);  // Small delay
                //                     continue;
                //                 }
                                
                //                 perror("Error receiving data");
                //                 break;
                //             }

                //             // Write received data to pipe
                //             size_t bytes_written = fwrite(buffer, 1, bytes_received, pipe);
                //             if (bytes_written != bytes_received) {
                //                 perror("Error writing to pipe");
                //                 break;
                //             }

                //             total_bytes_received += bytes_received;
                //         }

                //         // Flush and close
                //         fflush(pipe);
                //         pclose(pipe);
                //         close(ss_sock);

                //         printf("Streaming completed. Total bytes: %zu\n", total_bytes_received);
                //         continue;
                //     }
                else{
                    int recv_from_ss = recv(ss_sock, response_from_SS, 1024, 0);
                    if (recv_from_ss < 0) {
                        perror("Failed receiving response from SS");
                        close(ss_sock);
                        close(sockfd);
                        // free(response_from_SS);
                        exit(EXIT_FAILURE);
                    } else if (recv_from_ss == 0) {
                        printf("Connection closed by storage server.\n");
                        close(ss_sock);
                        close(sockfd);
                        // free(response_from_SS);
                        exit(EXIT_SUCCESS);
                    }
                    printf("%s\n", response_from_SS);
                    close(ss_sock);
                    // free(response_from_SS);
                    continue;

                }
        }
    }
    close(sockfd);
    return 0;
    
}