#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include "files/misc.h"
#include "files/ss.h"
#include "files/cl.h"
#include "files/lru.h"
#include "files/ss.h"


int main(){
    struct sockaddr_in server_address, client_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port= htons(NM_Client_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    client_address.sin_family = AF_INET;
    client_address.sin_port= htons(NM_SS_PORT_LISTEN);
    client_address.sin_addr.s_addr = INADDR_ANY;


    cache = (LRUCache *)malloc(sizeof(LRUCache));
    if(cache == NULL){
        perror("cache creation failed");
        exit(1);
    }
    printf("Cache created\n");
    initialise_LRUcache(cache);
    //we are creating resuable socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0){
        perror("server_socket creation failed");
        exit(1);
    }
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }
    
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket < 0){
        perror("client_socket creation failed");
        exit(1);
    }
    if (setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }

    //binding the sockets

    if(bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0){
        perror("server_socket binding failed");
        exit(1);
    }
    
    if(bind(client_socket, (struct sockaddr*)&client_address, sizeof(client_address)) < 0){
        perror("client_socket binding failed");
        exit(1);
    }

    // Listen for incoming connections
    if(listen(server_socket, 5) < 0){
        perror("server_socket listen failed");
        exit(1);
    }

    if(listen(client_socket, 5) < 0){
        perror("client_socket listen failed");
        exit(1);
    }

    //creating threads for server and client
    pthread_t server_thread, client_thread;


    if (pthread_create(&server_thread, NULL, listenForClients, (void*)&server_socket) != 0) {
        perror("Thread creation failed for Naming Server");
        close(server_socket);
        close(client_socket);
        return EXIT_FAILURE;
    }

    if (pthread_create(&client_thread, NULL,listenForStorageServers, (void*)&client_socket) != 0) {
        perror("Thread creation failed for Client");
        pthread_cancel(server_thread);
        pthread_join(server_thread, NULL);
        close(server_socket);
        close(client_socket);

        return EXIT_FAILURE;
    }

    if (pthread_join(server_thread, NULL) != 0) {
        perror("Error joining Naming Server thread");
    }

    if (pthread_join(client_thread, NULL) != 0) {
        perror("Error joining Client thread");
    }


    close(server_socket);
    close(client_socket);

    return 0;

    
}