#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define reset "\e[0m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define CYN "\e[0;36m"
#define reset "\e[0m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define reset "\e[0m"

#define ACK_SIZE 1000
#define SMALL_SIZE 100
#define NEW_SIZE 1000
#define BUFFER_SIZE 1024
#define NM_SERVER_PORT 8081
#define STREAM_BUFFER 8192

int SSNamingServerPort;
int SSClientPort;
char *namingServerIp;

int INITIAL_PORT = 7001;
int MAX_LENGTH = 4096;
int MAX_TOTAL_LENGTH = 1000000;

typedef struct {
    int socketDescriptor;
    char ipAddress[INET_ADDRSTRLEN];
} connectionInfo;

void sendACK(char *ackMessage) {
    int sock_fd;
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error in sendACK");
        return;
    }

    int reusePort = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if(reusePort < 0) {
        perror("setsockopt(SO_REUSEADDR) failed in sendACK");
        close(sock_fd);
        return;
    }

    struct sockaddr_in connection;
    memset(&connection, 0, sizeof(connection));
    connection.sin_family = AF_INET;
    connection.sin_port = htons(NM_SERVER_PORT);

    int convertAddress = inet_pton(AF_INET, namingServerIp, &connection.sin_addr);
    if(convertAddress <= 0) {
        perror("Invalid address in sendACK");
        return;
    }

    int connectStatus = connect(sock_fd, (struct sockaddr *)&connection, sizeof(connection));
    if(connectStatus < 0) {
        perror("Connection failed in sendACK");
        return;
    }
    send(sock_fd, ackMessage, strlen(ackMessage), 0);
    close(sock_fd);
}

void readFileContents(char **inputTokens, int sock) {
    char *filename = inputTokens[1];
    printf(YEL "Reading file %s\n" reset, filename);
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    long bytesRead = 0;
    char *buffer = (char *)malloc(BUFFER_SIZE* sizeof(char));
    if (buffer == NULL) {
        fprintf(stderr, RED "Memory allocation failed!\n" reset);
        fclose(file);
        return;
    }

    
    while((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        buffer[bytesRead] = '\0';
        send(sock, buffer, strlen(buffer), 0);
    }
    fclose(file);
    send(sock, "END", strlen("END"), 0);
    close(sock);

    printf(GRN "Sent the contents to the client!\n" reset);

    char* ackMessage = (char *)malloc(ACK_SIZE * sizeof(char));
    if (ackMessage == NULL) {
        printf(RED "Memory allocation failed!\n" reset);
        return;
    }

    ackMessage[0] = '\0';
    strcat(ackMessage, "ACK\nREAD\n");
    strcat(ackMessage, filename);
    sendACK(ackMessage);

    free(buffer);
}

void writeToFile(char **inputTokens, int numTokens, int sock) {
    char *content = (char *)malloc(NEW_SIZE * sizeof(char));
    if(content == NULL) {
        printf(RED "Memory allocation failed in writeToFile.\n" reset);
        return;
    }
    content[0] = '\0';

    int i = 2;
    while(i < numTokens) {
        strcat(content, " ");
        strcat(content, inputTokens[i]);
        i++;
    }

    char* filename = inputTokens[1];
    printf("Writing to the file '%s'\n", filename);
    FILE *file = fopen(filename, "a");
    if(file == NULL) {
        perror("Error opening file");
        return;
    }

    fputs(content, file);
    fclose(file);

    char *response = (char *)malloc(NEW_SIZE * sizeof(char));
    if(response == NULL) {
        printf(RED "Memory allocation failed in writeToFile.\n" reset);
        return;
    }
    response[0] = '\0';
    strcpy(response, "Text appended successfully.\n");
    send(sock, response, strlen(response), 0);
    close(sock);

    char* ackMessage = (char *)malloc(ACK_SIZE * sizeof(char));
    ackMessage = (char *)malloc(ACK_SIZE * sizeof(char));
    if(ackMessage == NULL) {
        printf(RED "Memory allocation failed in writeToFile.\n" reset);
        return;
    }
    ackMessage[0] = '\0';
    strcat(ackMessage, "ACK\nWRITE\n");
    strcat(ackMessage, filename);
    sendACK(ackMessage);
    free(content);
}
 

void createFile(char **inputTokens, int sock) {
    size_t len = strlen(inputTokens[1]);
    int isDirectory = (len > 0 && inputTokens[1][len - 1] == '/') ? 1 : 0;
    char *path = inputTokens[1];
    
    if(isDirectory) {
        if(access(inputTokens[1], F_OK) != -1) {
            printf(RED "Directory '%s' already exists.\n" reset, path);
            send(sock, "-1", strlen("-1"), 0);
            close(sock);
            return;
        }

        if(mkdir(inputTokens[1], 0777) == -1) {
            perror(RED "Error creating directory" reset);
            send(sock, "-1", strlen("-1"), 0);
            close(sock);
            return;
        }
        printf(GRN "Directory '%s' created successfully.\n" reset, path);
        send(sock, "CREATED", strlen("CREATED"), 0);
        close(sock);
    }
    else {
        if(access(path, F_OK) != -1) {
            printf(RED "File '%s' already exists.\n" reset, path);
            send(sock, "-1", strlen("-1"), 0);
            close(sock);
            return;
        }

        FILE *file = fopen(path, "w");
        if(file == NULL) {
            perror(RED "Error creating file" reset);
            send(sock, "-1", strlen("-1"), 0);
            close(sock);
            return;
        }
        printf(GRN "File '%s' created successfully.\n" reset, path);
        fclose(file);
        send(sock, "CREATED", strlen("CREATED"), 0);
        close(sock);
    }
}

void deleteFolder(char *folderPath) {
    char path[MAX_LENGTH];
    struct dirent *entry;
    struct stat statbuf;

    while (1) {
        DIR *dir = opendir(folderPath);
        if (!dir) {
            perror("Unable to open directory");
            return;
        }

        int hasContents = 0;

        while ((entry = readdir(dir)) != NULL) {
            // Skip `.` and `..` entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            hasContents = 1;

            snprintf(path, sizeof(path), "%s/%s", folderPath, entry->d_name);

            if (stat(path, &statbuf) == -1) {
                perror("Error reading file stats");
                closedir(dir);
                return;
            }

            if (S_ISDIR(statbuf.st_mode)) {
                deleteFolder(path);
            } 
            else {
                if (remove(path) == 0) {
                    printf("Deleted file: %s\n", path);
                } else {
                    perror("Failed to delete file");
                }
            }
        }

        closedir(dir);

        if (!hasContents) {
            if (rmdir(folderPath) == 0) {
                printf("Deleted folder: %s\n", folderPath);
            } 
            else {
                perror("Failed to delete folder");
            }
            break;
        }
    }
}

void deleteFileFolder(char **inputTokens, int sock) {
    size_t len = strlen(inputTokens[1]);
    int isDirectory = (len > 0 && inputTokens[1][len - 1] == '/') ? 1 : 0;
    char *path = inputTokens[1];

    if(!isDirectory) {
        // Delete the file
        if(remove(path) == 0) {
            printf(GRN "File '%s' deleted successfully.\n" reset, path);
            send(sock, "DELETED FILE", strlen("DELETED FILE"), 0);
            close(sock);
            return;
        }
        perror("Error deleting file");
    }
    else{
        // Delete the folder
        deleteFolder(path);
        send(sock, "DELETED FOLDER", strlen("DELETED FOLDER"), 0);
        close(sock);
    }

}

void fetchInfo(char **inputTokens, int sock) {
    struct stat fileInfo;

    int status = stat(inputTokens[1], &fileInfo);
    if (status != 0) {
        perror(RED "Error getting file info" reset);
        return;
    }

    char *sendInfo = (char *)malloc(BUFFER_SIZE * sizeof(char));
    if (sendInfo == NULL) {
        printf(RED "Memory allocation failed!\n" reset);
        return;
    }
    sendInfo[0] = '\0';
    strcat(sendInfo, "\nFile Size: ");

    char *size = (char *)malloc(SMALL_SIZE * sizeof(char));
    if (size == NULL) {
        printf(RED "Memory allocation failed!\n" reset);
        return;
    }

    sprintf(size, "%ld", fileInfo.st_size);
    sprintf(sendInfo,"%s bytes \nFile Permissions: ", size);

    char permissions[11]; 
    permissions[0] = S_ISDIR(fileInfo.st_mode) ? 'd' : '-';
    int modes[] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    char symbols[] = {'r', 'w', 'x'};
    for (int i = 0; i < 9; i++) {
        permissions[i + 1] = (fileInfo.st_mode & modes[i]) ? symbols[i % 3] : '-';
    }
    permissions[10] = '\0';

    strcat(sendInfo, permissions);
    strcat(sendInfo, "\n");

    printf("Sending file info to the client\n");

    char modTime[30], accTime[30];
    strftime(modTime, sizeof(modTime), "%Y-%m-%d %H:%M:%S", localtime(&fileInfo.st_mtime));
    strftime(accTime, sizeof(accTime), "%Y-%m-%d %H:%M:%S", localtime(&fileInfo.st_atime));

    strcat(sendInfo, "Last Modified: ");
    strcat(sendInfo, modTime);
    strcat(sendInfo, "\n");

    strcat(sendInfo, "Last Accessed: ");
    strcat(sendInfo, accTime);
    strcat(sendInfo, "\n");

    if (send(sock, sendInfo, strlen(sendInfo), 0) < 0) {
        perror(RED "Failed to send file info to client" reset);
    }
    close(sock);

    char ackMessage[ACK_SIZE];
    snprintf(ackMessage, sizeof(ackMessage), "ACK\nINFO\n%s", inputTokens[1]);
    sendACK(ackMessage);
    free(sendInfo);
}

void streamFile(char **inputTokens, int sock) {
    char *filename = inputTokens[1];
    printf(YEL "Streaming file %s\n" reset, filename);
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    long bytesRead = 0;
    char *buffer = (char *)malloc(STREAM_BUFFER * sizeof(char));
    if (buffer == NULL) {
        fprintf(stderr, RED "Memory allocation failed!\n" reset);
        fclose(file);
        return;
    }

    while((bytesRead = fread(buffer, 1, STREAM_BUFFER, file)) > 0) {
        // printf("Sending %ld bytes\n", bytesRead);x
        buffer[bytesRead] = '\0';
        send(sock, buffer, bytesRead, 0);
    }

    fclose(file);
    close(sock);

    printf(GRN "Sent the contents to the client!\n" reset);
    char* ackMessage = (char *)malloc(ACK_SIZE * sizeof(char));
    if (ackMessage == NULL) {
        printf(RED "Memory allocation failed!\n" reset);
        return;
    }

    ackMessage[0] = '\0';
    strcat(ackMessage, "ACK\nSTREAM\n");
    strcat(ackMessage, filename);
    sendACK(ackMessage);

    free(buffer);
}

void NMreadFileContents(char **inputTokens, int sock) {
    char *filename = inputTokens[1];
    printf(YEL "Reading file %s\n" reset, filename);
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    long bytesRead = 0;
    char *buffer = (char *)malloc(BUFFER_SIZE* sizeof(char));
    if (buffer == NULL) {
        fprintf(stderr, RED "Memory allocation failed!\n" reset);
        fclose(file);
        return;
    }

    bytesRead = fread(buffer, 1, BUFFER_SIZE, file);
    send(sock, buffer, strlen(buffer), 0);
    fclose(file);
    close(sock);
    printf(GRN "Sent the contents to the NM! :" reset);
    printf("%s\n", buffer);
}

void NMwriteToFile(char **inputTokens, int numTokens, int sock) {
    char *content = (char *)malloc(BUFFER_SIZE * sizeof(char));
    if(content == NULL) {
        printf(RED "Memory allocation failed in writeToFile.\n" reset);
        return;
    }
    memset(content, 0, BUFFER_SIZE);

    int i = 2;
    while(i < numTokens) {
        strcat(content, " ");
        strcat(content, inputTokens[i]);
        i++;
    }

    char* filename = inputTokens[1];
    printf("Writing to the file '%s'\n", filename);
    FILE *file = fopen(filename, "wb");
    if(file == NULL) {
        perror("Error opening file");
        return;
    }

    fputs(content, file);
    fclose(file);

    char *response = (char *)malloc(NEW_SIZE * sizeof(char));
    if(response == NULL) {
        printf(RED "Memory allocation failed in writeToFile.\n" reset);
        return;
    }
    response[0] = '\0';
    strcpy(response, "WRITTEN\n");
    send(sock, response, strlen(response), 0);
    close(sock);
    free(content);
    printf(GRN "Contents Written Successfully!\n" reset);
}

void operationsHandler(char **inputTokens, int numTokens, int sock) {
    if (strcmp(inputTokens[0], "CREATE") == 0) {
        createFile(inputTokens, sock);
        return;
    }
    else if (strcmp(inputTokens[0], "READ") == 0) {
        readFileContents(inputTokens, sock);
        return;
    }
    else if (strcmp(inputTokens[0], "WRITE") == 0) {
        writeToFile(inputTokens, numTokens, sock);
        return;
    }
    else if (strcmp(inputTokens[0], "DELETE") == 0) {
        deleteFileFolder(inputTokens, sock);
        return;
    }
    else if (strcmp(inputTokens[0], "INFO") == 0) {
        fetchInfo(inputTokens, sock);
        return;
    }
    else if(strcmp(inputTokens[0], "STREAM") == 0) {
        streamFile(inputTokens, sock);
        return;
    }
    else if(strcmp(inputTokens[0], "NMREAD") == 0) {
        NMreadFileContents(inputTokens, sock);
        return;
    }
    else if(strcmp(inputTokens[0], "NMWRITE") == 0) {
        NMwriteToFile(inputTokens, numTokens, sock);
        return;
    }
    else {
        printf("Invalid Command.\n");
        return;
    }
}

int findFreePort() {
    int sockfd;
    struct sockaddr_in addr;
    int port = INITIAL_PORT;

    while (port < 65535) {
        if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation error in findFreePort");
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr = (struct sockaddr_in){
            .sin_family = AF_INET,
            .sin_addr.s_addr = htonl(INADDR_ANY),
            .sin_port = htons(port)
        };

        int bindStatus = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
        if (bindStatus < 0) {
            if (errno == EADDRINUSE) {
                close(sockfd);
                port++;
            } 
            else {
                perror("Bind failed in findFreePort");
                close(sockfd);
            }
            close(sockfd);
        } 
        else {
            INITIAL_PORT = port + 1;
            close(sockfd);
            return port;
        }
    }
    return -1;
}

void listFiles(char *basepath, char *currentPath, char **paths, int *length) {
    DIR *directory = opendir(basepath);
    struct dirent *directoryEntry;

    if(directory == NULL) {
        perror("opendir() error in listFiles");
        return;
    }

    char path[MAX_LENGTH];
    struct stat fileStat;

    while((directoryEntry = readdir(directory)) != NULL) {
        if(strcmp(directoryEntry->d_name, "..") != 0 && strcmp(directoryEntry->d_name, ".") != 0) {
            if(strlen(currentPath) != 0) {
                snprintf(path, sizeof(path), "%s%s", currentPath, directoryEntry->d_name);
            } 
            else {
                snprintf(path, sizeof(path), "%s", directoryEntry->d_name);                
            }

            char fullPath[MAX_LENGTH];
            snprintf(fullPath, sizeof(fullPath), "%s%s%s", basepath, (basepath[strlen(basepath) - 1] == '/') ? "" : "/", directoryEntry->d_name);

            if(stat(fullPath, &fileStat) != -1) {
                if(S_ISDIR(fileStat.st_mode)) {
                    snprintf(*paths + *length, MAX_TOTAL_LENGTH - *length, "%s/\n", path);
                    *length += strlen(path) + 2;

                    char newCurrentPath[MAX_LENGTH];
                    snprintf(newCurrentPath, sizeof(newCurrentPath), "%s/", path);
                    listFiles(fullPath, newCurrentPath, paths, length);
                }
                else {
                    snprintf(*paths + *length, MAX_TOTAL_LENGTH - *length, "%s\n", path);
                    *length += strlen(path) + 1;
                }
            }
        }
    }
    closedir(directory);
}

void selectPaths(char *paths, char *selectedPaths) {
    char * token;
    int num, idx;
    char buffer[MAX_TOTAL_LENGTH];

    printf(BLU "Enter '0' to select all paths, or specify the number of paths to select: " reset);
    scanf("%d", &num);
    getchar();

    selectedPaths[0] = '\0';

    if(num == 0) {
        strcpy(selectedPaths, paths);
    } 
    else {
        printf(BLU "Enter the indices of the paths to select: " reset);
        int i = 0;
        while(i < num) {
            scanf("%d", &idx);
            strcpy(buffer, paths);
            token = strtok(buffer, "\n");

            int j = 1;
            while(token != NULL) {
                if(j == idx) {
                    strcat(selectedPaths, token);
                    strcat(selectedPaths, "\n");
                    break;
                }
                token = strtok(NULL, "\n");
                j++;
            }
            i++;
        }
        getchar();
    }

}       

void listingPathsToNM() {
    char *serverIP = (char *)malloc(INET_ADDRSTRLEN);
    if(serverIP == NULL) {
        printf("Memory allocation failed for serverIP.\n");
        return ;
    }

    printf("Enter the IP address of the Naming Server: ");
    fgets(serverIP, INET_ADDRSTRLEN, stdin);
    serverIP[strcspn(serverIP, "\n")] = 0;

    namingServerIp = (char *)malloc(INET_ADDRSTRLEN);
    if(namingServerIp == NULL) {
        printf("Memory allocation failed for nameServerIP.\n");
        return ;
    }
    namingServerIp[0] = '\0';

    strcpy(namingServerIp, serverIP);

    char path[MAX_LENGTH];
    char* findDir = getcwd(path, sizeof(path));
    if(findDir == NULL) {
        perror("getcwd() error in listingPathsToNM");
        return;
    }
    char *allPaths = (char *)malloc(MAX_LENGTH);
    int length = 0;
    allPaths[0] = '\0';
    listFiles(path, "", &allPaths, &length);

    char *allPathsCopy = (char *)malloc(MAX_LENGTH);
    char *firstMessageToNM = (char *)malloc(MAX_LENGTH);

    strcpy(allPathsCopy, allPaths);

    char *token = strtok(allPaths, "\n");
    
    int idx = 1;
    while(token != NULL) {
        printf("%d %s\n", idx, token);
        token = strtok(NULL, "\n");
        idx++;
    }

    strcpy(allPaths, allPathsCopy);
    char *selectedPaths = (char *)malloc(MAX_LENGTH);
    selectPaths(allPaths, selectedPaths);

    int SSNamingServerSocket;
    if((SSNamingServerSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error in listingPathsToNM");
        return;
    }

    int reuseSockert = setsockopt(SSNamingServerSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if(reuseSockert < 0) {
        perror("setsockopt(SO_REUSEADDR) failed in listingPathsToNM");
        close(SSNamingServerSocket);
        return;
    }

    struct sockaddr_in SSNamingServerAddr;
    memset(&SSNamingServerAddr, '0', sizeof(SSNamingServerAddr));
    SSNamingServerAddr.sin_family = AF_INET;
    SSNamingServerAddr.sin_port = htons(NM_SERVER_PORT);

    int convertAddress = inet_pton(AF_INET, serverIP, &SSNamingServerAddr.sin_addr);
    if(convertAddress <= 0) {
        perror("Invalid address in listingPathsToNM");
        return;
    }

    int connectStatus = connect(SSNamingServerSocket, (struct sockaddr *)&SSNamingServerAddr, sizeof(SSNamingServerAddr));
    if(connectStatus < 0) {
        perror("Connection failed in listingPathsToNM");
        return;
    }

    snprintf(firstMessageToNM, MAX_TOTAL_LENGTH, "ssinit\n%d %d\n", SSNamingServerPort, SSClientPort);
    strcat(firstMessageToNM, selectedPaths);
    send(SSNamingServerSocket, firstMessageToNM, strlen(firstMessageToNM), 0);

    char* buffer = (char *)malloc(BUFFER_SIZE* sizeof(char));
    if(buffer == NULL) {
        perror("Memory allocation failed in listingPathsToNM");
        close(SSNamingServerSocket);
        return;
    }

    memset(buffer, 0, BUFFER_SIZE);
    ssize_t readStatus = read(SSNamingServerSocket, buffer, BUFFER_SIZE - 1);
    if(readStatus < 0) {
        free(buffer);
        close(SSNamingServerSocket);
        return;
    }
    buffer[readStatus] = '\0';
    printf("NM -> %s\n", buffer);
    mkdir("bkps", 0777);
    close(SSNamingServerSocket);
}

char *readMSG(int sock) {
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    memset(buffer, 0, BUFFER_SIZE);                     
    ssize_t bytesRead = read(sock, buffer, BUFFER_SIZE - 1); 
    if (bytesRead < 0) {
        free(buffer);
        close(sock);
        return "-1";
    }

    buffer[bytesRead] = '\0';
    return buffer;
}

char **allocateTokens(int tokenCount, int tokenSize) {
    char **tokens = (char **)calloc(tokenCount, sizeof(char *));
    if (tokens == NULL) {
        printf(RED "Memory allocation failed for tokens!\n" reset);
        return NULL;
    }
    for (int i = 0; i < tokenCount; i++) {
        tokens[i] = (char *)calloc(tokenSize, sizeof(char));
        if (tokens[i] == NULL) {
            printf(RED "Memory allocation failed for token[%d]!\n" reset, i);
            for (int j = 0; j < i; j++) {
                free(tokens[j]);
            }
            free(tokens);
            return NULL;
        }
    }
    return tokens;
}

void freeTokens(char **tokens, int tokenCount) {
    if (tokens == NULL) return;
    for (int i = 0; i < tokenCount; i++) {
        if (tokens[i] != NULL) {
            free(tokens[i]);
        }
    }
    free(tokens);
}

void parseInput(char *input, int sock) {
    char **tokens = allocateTokens(NEW_SIZE, SMALL_SIZE);
    if (tokens == NULL) {
        return;
    }
    int numTokens = 0;
    char *token = strtok(input, " ");
    while (token != NULL) {
        if (numTokens >= NEW_SIZE) {
            printf(RED "Too many tokens, input exceeds limit!\n" reset);
            freeTokens(tokens, NEW_SIZE);
            return;
        }
        strcpy(tokens[numTokens++], token);
        token = strtok(NULL, " ");
    }

    operationsHandler(tokens, numTokens, sock);

    freeTokens(tokens, NEW_SIZE);
}

void *handleClientConnection(void *arg) {
    connectionInfo *info = (connectionInfo *)arg;
    char ip_address[INET_ADDRSTRLEN];
    strcpy(ip_address, info->ipAddress);
    int socket = info->socketDescriptor;
    free(info);

    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    if(buffer == NULL) {
        close(socket);
        return NULL;
    }
    memset(buffer, 0, BUFFER_SIZE);

    strcpy(buffer, readMSG(socket));

    printf(GRN "Client -> %s\n" reset, buffer);
    parseInput(buffer, socket);
    free(buffer);
    close(socket);
}

void *handleNamingServerConnection(void *arg) {
    connectionInfo *info = (connectionInfo *)arg;
    char ip_address[INET_ADDRSTRLEN];
    strcpy(ip_address, info->ipAddress);
    int socket = info->socketDescriptor;
    free(info);

    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    if(buffer == NULL) {
        close(socket);
        return NULL;
    }
    memset(buffer, 0, BUFFER_SIZE);

    strcpy(buffer, readMSG(socket));

    printf(GRN "NS -> %s\n" reset, buffer);
    parseInput(buffer, socket);
    free(buffer);
    close(socket);
}

void *listenClients(void *SSClient_fd) {
    int NMClientSocket = *(int *)SSClient_fd;
    int newSocket;
    struct sockaddr_in NMClientAddr;
    char ipBuffer[INET_ADDRSTRLEN];

    while(1) {
        printf(YEL "Listening to requests from Clients...\n" reset);
        int addrLength = sizeof(NMClientAddr);
        newSocket = accept(NMClientSocket, (struct sockaddr *)&NMClientAddr, (socklen_t *)&addrLength);

        strncpy(ipBuffer, inet_ntoa(NMClientAddr.sin_addr), INET_ADDRSTRLEN);
        ipBuffer[INET_ADDRSTRLEN - 1] = '\0';

        if(newSocket < 0) {
            perror("accept in listenClients");
            return NULL;
        }
        else {
            printf(CYN "Connection established with Client at IP address %s and port %d\n" reset, ipBuffer, ntohs(NMClientAddr.sin_port));
            connectionInfo *info = (connectionInfo *)malloc(sizeof(connectionInfo));
            if(info == NULL) {
                printf(RED "Memory allocation failed in listenClients\n" reset);
                return NULL;
            }
            info->socketDescriptor = newSocket;
            strcpy(info->ipAddress, ipBuffer);
            pthread_t clientThread;
            if (pthread_create(&clientThread, NULL, handleClientConnection, (void *)info) != 0) {
                perror("pthread_create error in listenClients");
                free(info); 
                close(newSocket);
                continue;
            }
            pthread_join(clientThread, NULL);
        }
    }
}

void *listenNamingServer(void *SSNamingServer_fd) {
    int NMStorageServerSocket = *(int *)SSNamingServer_fd;
    struct sockaddr_in NMStorageServerAddr;
    int newSocket;
    char ipBuffer[INET_ADDRSTRLEN];

    while(1) {
        printf(YEL "Listening to requests from Naming Server...\n" reset);
        int addrLength = sizeof(NMStorageServerAddr);
        newSocket = accept(NMStorageServerSocket, (struct sockaddr *)&NMStorageServerAddr, (socklen_t *)&addrLength);

        strncpy(ipBuffer, inet_ntoa(NMStorageServerAddr.sin_addr), INET_ADDRSTRLEN);
        ipBuffer[INET_ADDRSTRLEN - 1] = '\0';

        if(newSocket < 0) {
            perror("accept in listenNamingServer");
            return NULL;
        }
        else {
            printf(CYN "Connection established with Naming Server at IP address %s and port %d\n" reset, ipBuffer, ntohs(NMStorageServerAddr.sin_port));
            connectionInfo *info = (connectionInfo *)malloc(sizeof(connectionInfo));
            if(info == NULL) {
                printf(RED "Memory allocation failed in listenNamingServer\n" reset);
                return NULL;
            }
            info->socketDescriptor = newSocket;
            strcpy(info->ipAddress, ipBuffer);
            pthread_t namingServerThread;
            if (pthread_create(&namingServerThread, NULL, handleNamingServerConnection, (void *)info) != 0) {
                perror("pthread_create error in listenNamingServer");
                free(info); 
                close(newSocket);
                continue;
            }
            pthread_join(namingServerThread, NULL);
        }   
    }
}

int createAndBindSocket(int port) {
    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    int reuseSocket = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseSocket, sizeof(reuseSocket)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sockfd);
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 5) < 0) {
        perror("Listen failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int main() {
    SSNamingServerPort = findFreePort();
    if(SSNamingServerPort == -1) {
        printf("Failed to find a free port for Naming Server.\n");
        return -1;
    }
    SSClientPort = findFreePort();
    if(SSClientPort == -1) {
        printf("Failed to find a free port for Client.\n");
        return -1;
    }

    printf("Storage Server Port (Naming Server): %d\nStorage Server Port (Client): %d\n", SSNamingServerPort, SSClientPort);

    listingPathsToNM();

    int SSNamingServer_fd = createAndBindSocket(SSNamingServerPort);
    if (SSNamingServer_fd < 0) {
        return EXIT_FAILURE;
    }

    int SSClient_fd = createAndBindSocket(SSClientPort);
    if (SSClient_fd < 0) {
        close(SSNamingServer_fd);
        return EXIT_FAILURE;
    }

    pthread_t namingServerThread, clientThread;

    if (pthread_create(&namingServerThread, NULL, listenNamingServer, &SSNamingServer_fd) != 0) {
        perror("Thread creation failed for Naming Server");
        close(SSNamingServer_fd);
        close(SSClient_fd);
        return EXIT_FAILURE;
    }

    if (pthread_create(&clientThread, NULL, listenClients, &SSClient_fd) != 0) {
        perror("Thread creation failed for Client");
        pthread_cancel(namingServerThread);
        pthread_join(namingServerThread, NULL);
        close(SSNamingServer_fd);
        close(SSClient_fd);
        return EXIT_FAILURE;
    }

    if (pthread_join(namingServerThread, NULL) != 0) {
        perror("Error joining Naming Server thread");
    }

    if (pthread_join(clientThread, NULL) != 0) {
        perror("Error joining Client thread");
    }

    close(SSClient_fd);
    close(SSNamingServer_fd);
    return 0;
}