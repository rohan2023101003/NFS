#ifndef SS_H
#define SS_H

#include <string.h>
#include "misc.h"
#include "cl.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

//BOOK KEEPING

// Logging approach with comprehensive error handling
void custom_print(const char *format, ...) {
    // Static mutex for thread-safety
    static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    // Timestamp buffer
    char timestamp[64];
    time_t now;
    struct tm *t;

    // Acquire mutex
    pthread_mutex_lock(&log_mutex);

    // Get current timestamp
    time(&now);
    t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", t);

    // Open log file
    FILE *file = fopen("book_keeping.log", "a");
    if (!file) {
        perror("Failed to open log file");
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    // Prepare va_lists
    va_list args, args_copy;
    va_start(args, format);
    va_copy(args_copy, args);

    // Print to stdout with timestamp
    printf("%s", timestamp);
    vprintf(format, args);

    // Write to file with timestamp
    fprintf(file, "%s", timestamp);
    vfprintf(file, format, args_copy);

    // Cleanup
    va_end(args);
    va_end(args_copy);

    // Close file and release mutex
    fclose(file);
    pthread_mutex_unlock(&log_mutex);
}

// Macro to replace standard printf
#define printf(format, ...) custom_print(format, ##__VA_ARGS__)


// int parse_init_message(char *init_message, char *ip_address);
static bool isValidInitHeader(const char *header) {
    return header && strcmp(header, "ssinit") == 0;
}
typedef struct ServerPorts {
    int nm_port;
    int cl_port;
} ServerPorts;

typedef struct ParseResult {
    bool success;
    ServerPorts ports;
    char **paths;
    size_t path_count;
} ParseResult;

int no_of_ss;
// Helper function to parse port numbers
static bool parsePortNumbers(char *port_line, ServerPorts *ports) {
    if (!port_line || !ports) return false;
    return sscanf(port_line, "%d %d", &ports->nm_port, &ports->cl_port) == 2;
}

int parse_init_message(char* init_message, char* ip_address);
int parse_init_message(char *init_message, char *ip_address){
    if (!init_message || !ip_address) {
        printf("Error: Invalid input parameters\n" );
        return 0;
    }
    char *header = strtok(init_message, "\n");

    if (!isValidInitHeader(header)) {
        printf("Error: Invalid init message header\n" );
        return 0;
    }
      // Parse port numbers
    ServerPorts ports;
    char *port_line = strtok(NULL, "\n");
    // if (!parsePortNumbers(port_line, &ports)) {
    //     printf(RED "Error: Failed to parse port numbers\n" reset);
    //     return 0;
    // }
    if(port_line == NULL || sscanf(port_line, "%d %d", &ports.nm_port, &ports.cl_port) != 2){
        printf("Error: Port numbers not found\n");
        return 0;
    }

    // Process paths
    bool paths_processed = false;
    
    char *path;
    while ((path = strtok(NULL, "\n")) != NULL) {
        sem_t write_mutex;
         // Zero initialize struct
        char ipaddress[100];

        // Copy IP with bounds checking
        if (strlen(ip_address) >= 100) {
            printf("Error: IP address too long\n");
            break;
        }
        sem_t read_mutex;
        strcpy(ipaddress, ip_address);

    //    entry_info vs = {0};
        
        // Initialize semaphores
        if (sem_init(&write_mutex, 0, 1) != 0) {
            printf("Error: Failed to initialize write semaphore\n");
            break;
        }
        if (sem_init(&read_mutex, 0, 1) != 0) {
            printf("Error: Failed to initialize read semaphore\n");
            sem_destroy(&write_mutex);
            break;
        }
        entry_info vs ;
        strncpy(vs.ip, ipaddress,sizeof(vs.ip) - 1);
        vs.ip[sizeof(vs.ip) - 1] = '\0';
        vs.nm_port = ports.nm_port;
        vs.cl_port = ports.cl_port;
        vs.no_readers = 0;
        vs.is_writing = 0;
        vs.write_mutex = write_mutex;
        vs.read_mutex = read_mutex;
        paths_processed = true;
        putInLRUCache(cache, path, vs);
    }

    if (!paths_processed) {
        printf( "Warning: No paths received from storage server\n" );
    }
        // Update server list and counter
    insertNode(&head_list, ip_address, ports.nm_port, ports.cl_port);
    no_of_ss++;
        
        printf( "NM > Storage server added successfully\n" );
        printf("Connected servers:\n");
        displayList(head_list);
        printf( "NM > Total storage servers: %d\n" , no_of_ss);    
    
    return 1;

}

// Enum for message types
typedef enum {
   MSG_INIT,
   MSG_ACK,
   MSG_UNKNOWN
} MessageType;

// Helper to identify message type
static MessageType getMessageType(const char *buffer) {
   if (strncmp(buffer, "ssinit", 6) == 0) return MSG_INIT;
   if (strncmp(buffer, "ACK", 3) == 0) return MSG_ACK;
   return MSG_UNKNOWN;
}

// Helper to process ACK message
// static void handleAckMessage(const char *buffer, AckMessage *ack) {
//    char *token = strtok((char*)buffer, "\n");
//    token = strtok(NULL, "\n");
//    strncpy(ack->operation, token, sizeof(ack->operation));
   
//    token = strtok(NULL, "\n");
//    strncpy(ack->path, token, sizeof(ack->path));
// }

// Helper to update reader/writer counts
static void updateAccessCounts(const char *operation, const char *path) {
   entry_info *vs = getFromLRUCache(cache, path);
   if (!vs) return;

   if (strcmp(operation, "READ") == 0 || strcmp(operation, "GETINFO") == 0) {
       sem_wait(&(vs->read_mutex));
       vs->no_readers--;
       sem_post(&(vs->read_mutex));
       printf( "Reader completed. Readers for %s: %d\n" , 
              path, vs->no_readers);
   }
   else if (strcmp(operation, "WRITE") == 0) {
       sem_wait(&(vs->write_mutex));
       vs->is_writing = 0;
       sem_post(&(vs->write_mutex));
       printf( "Writer completed. is_writing for %s: %d\n" , 
              path, vs->is_writing);
   }
   //remove and then add to the cache
    removeFileFromCache(cache, path);
    putInLRUCache(cache, path, *vs);

}

char* process_SS_msg(int sock, char *ip_address) {
    // Read message from socket
    char buffer[1024] = {0};
    ssize_t bytes = recv(sock, buffer, 1024 - 1, 0);

    // Handle receive errors
    if (bytes < 0) {
        return "NM > Message receive failed";
    }

    // Handle disconnection
    if (bytes == 0) {
        printf("NM > Storage server %s disconnected\n", ip_address);
        no_of_ss--;
        delete_by_ip(cache->hashmap, ip_address);
        return NULL;
    }

    // Process message based on type
    MessageType msgType = getMessageType(buffer);
    // printf("msgType: %d\n", msgType);
    switch (msgType) {
        case MSG_INIT: {
            printf("%s", buffer);
            if (parse_init_message(buffer, ip_address)) {
                printf("ssinit completed successfully\n");
                return "ssinit completed successfully";
            }
            printf("ssinit failed\n");
            return "ssinit failed";
        }
        case MSG_ACK: {
            printf("ACK received\n");
            char* ack_op = strtok(buffer, "\n");
            if(ack_op != NULL){
                ack_op = strtok(NULL, "\n");
            }
            char* ack_path = strtok(NULL, "\n");
            printf("SS > ACK received for '%s' operation\n", ack_op);

            updateAccessCounts(ack_op, ack_path);
            break;
        }
        case MSG_UNKNOWN:
        default: {
            // Do nothing
            break;
        }
    }

    printf("SS> %s\n", buffer);
    return "hello ss\n";
}

typedef struct ServerContext {
   int socket;
   char ip[INET_ADDRSTRLEN];
   bool is_connected;
} ServerContext;

void *handleStorageServerConnection(void* arg){
    enum ServerState {
       INIT,
       PROCESSING,
       CLEANUP,
       ERROR
   };
   
   connection_info *info = (connection_info *)arg;
   ServerContext ctx = {
       .socket = info->port,
       .is_connected = true
   };
   strncpy(ctx.ip, info->ip, INET_ADDRSTRLEN);
   
   free(info);
    enum ServerState state = INIT;
   char *status_msg = NULL;
   int bakcup = 0;
   while (state != ERROR) {
       switch(state) {
           case INIT:
               printf( "Initializing connection with %s\n" , ctx.ip);
               state = PROCESSING;
               break;
               
           case PROCESSING:
               status_msg = process_SS_msg(ctx.socket, ctx.ip);
               if(send(ctx.socket, status_msg, strlen(status_msg), 0) < 0) {
                   printf( "Error sending message to %s\n" , ctx.ip);
                   state = ERROR;
               }
               else{
                     state = CLEANUP;
               }
               break;
               
           case CLEANUP:
               close(ctx.socket);
               ctx.is_connected = false;
                if(no_of_ss == 3){
                    printf( "Starting to create the backups now. \n");
                    backup_init();
                    bakcup=1;
                }
               state = ERROR;
               break;
               
           case ERROR:
            //    printf( "Error handling connection with %s\n" , ctx.ip);
               if (ctx.is_connected) {
                   close(ctx.socket);
                   ctx.is_connected = false;
               }
                if(no_of_ss == 3){
                    printf( "Starting to create the backups now. \n");
                    backup_init();
                    bakcup=1;
                }
               break;
       }
   }
    if(state == ERROR){
        // printf( "Error handling connection with %s\n" , ctx.ip);
           if (ctx.is_connected) {
                    close(ctx.socket);
            }
            if( bakcup !=1 && no_of_ss == 3){
                printf( "Starting to create the backups now. \n");
                backup_init();
            }
    }
   return NULL;

}
void* listenForStorageServers(void* arg){
   int server_fd = *(int *)arg;
   int *socket = NULL;
   connection_info *conn = NULL;
   pthread_t tid;
   for(;;){
       socket = NULL;
       conn = NULL;
       struct sockaddr_in addr = {0};
       char ip[INET_ADDRSTRLEN] = {0};
       socklen_t addr_len = sizeof(addr);
       if (!(socket = (int*)malloc(sizeof(int))))
       {
           printf("NM > Socket allocation failed\n");
           return NULL;
       }
       *socket = accept(server_fd, (struct sockaddr*)&addr, &addr_len);
       if ((*socket) < 0)
       {
           printf("NM > Connection failed\n");
           free(socket);
           continue;
       }
       else
       {
           inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
           printf("NM > Connected to storage server with IP %s\n", ip);
           if (!(conn = (connection_info*)malloc(sizeof(connection_info))))
           {
               printf(RED "NM > Connection info allocation failed\n" reset);
               free(socket);
               return NULL;
           }
           conn->port = *socket;
           strncpy(conn->ip, ip, INET_ADDRSTRLEN);

           // Create handler thread
           if (pthread_create(&tid, NULL, handleStorageServerConnection, (void*)conn) != 0)
           {
               printf(RED "NM > Thread creation failed for %s\n" reset, ip);
               free(conn);
               free(socket);
               continue;
           }

           // Success - continue to next iteration
           free(socket);
           continue;
       }
   }
   return NULL;
}
#endif // SS_HANDLER_H