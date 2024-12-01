#ifndef CL_H
#define CL_H

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
#include <sys/types.h>
#include "misc.h"
#include "ss.h"
#include "lru.h"



// structure to store the information of a storage server
typedef struct {
    char ip[16];
    int port;
} connection_info;




#define WORD_SIZE_IN_INPUT 100
#define NUM_WORDS_IN_INPUT 100



char *parseInput(char *input);
char *readClientMessage(int sock);
void *handleClientConnection(void *arg);
void *listenForClients(void *arg);
char *operation_handler(char **tokens, int num_tokens);
char *read_or_getinfo_file(char *file);
char *write_file(char *file);
int connect_to_server(char *ip, int port);
char *delete_file(char *file);
char *list();
char *copy(char *src, char *dest);
char *create_file(char *file);
char *copy_directory(char *src, char *dest);
char *copy_file(char *src, char *dest);
char *create_dir_in_dest_server(entry_info *entry, char *dest);
char *create_file_in_dest_server(entry_info *entry, char *dest);
char *read_and_save_file(entry_info *entry, char *src);
char *write_file_in_dest_server(entry_info *entry, char *dest, char *src);




char *parseInput(char *input)
{
  // Func to parse the input

  char *tokens[NUM_WORDS_IN_INPUT];
  int num_tokens = 0;
  
  char *token = strtok(input, " ");
  //here if first token is "WRITE" then dont tokenize the after the second token like this WRITE a.txt nnnfnf then tokens are WRITE, a.txt, nnnfnf
  if(strcmp(token, "WRITE") == 0){
    tokens[num_tokens++] = token;
    token = strtok(NULL, " ");
    tokens[num_tokens++] = token;
    token = strtok(NULL, " ");
    char *temp = strtok(NULL, "\n");
    tokens[num_tokens++] = temp;
  }
  else{
    while (token != NULL)
    {
      tokens[num_tokens++] = token;
      token = strtok(NULL, " ");
    }
  }
  if(num_tokens == 0){
    return "NULL";
  }
  for(int i = 0; i < num_tokens; i++){
    printf("Token %d : %s\n", i, tokens[i]);
  }
  return operation_handler(tokens, num_tokens);
}

char *readClientMessage(int sock)
{
  //func to read the message from the client

  char buffer[1024] = {0};
  int read_bytes = recv(sock, buffer, 1024, 0);
  if (read_bytes < 0)
  {
    perror("Error in receiving the message");
    return NULL; // Or handle the error as you see fit
  }
  // printf("read bytes : %d\n", read_bytes);
  if(read_bytes == 0){
    printf("Client has closed the connection\n");
    return NULL;
  }
  if(strcmp(buffer, " ") == 0){
    return "NULL";
  }
  printf("Client > %s\n", buffer);
  //here because of empty then directly return invalid input
  
  return parseInput(buffer);
}

void *handleClientConnection(void *arg)
{
  // Func to handle the client connection

  connection_info *info = (connection_info *)arg;
  int sock = info->port;
  char ip_buffer[16];
  strncpy(ip_buffer, info->ip, 16);

  while (1)
  {
    char *response = readClientMessage(sock);
    if (response == NULL)
    {
      printf("Client %s disconnected\n", ip_buffer);
      break;
    }
    // printf("Response : %s\n", response);
    // printf("Sending response to client\n");
    // printf("length of response : %ld\n", strlen(response));
    // printf("ip %s\n", info->ip);
    if (send(sock, response, strlen(response), 0) < 0)
    {
      perror("Error in sending the message");
      break;
    }
  }
  close(sock);
  return NULL;
}

void *listenForClients(void *arg)
{
  // Func to listen for clients

  int server_fd = *(int *)arg;

  while (1)
  {
    struct sockaddr_in client_address;
    int addrlen = sizeof(client_address);
    int client_socket = accept(server_fd, (struct sockaddr *)&client_address, (socklen_t *)&addrlen);
    if (client_socket < 0)
    {
      perror("Error in accepting the client");
      continue;
    }
    char ip_buffer[16];
    inet_ntop(AF_INET, &client_address.sin_addr, ip_buffer, 16);
    printf("Client %s connected\n", ip_buffer);

    connection_info *info = (connection_info *)malloc(sizeof(connection_info));
    if (info == NULL)
    {
      perror("Error in allocating memory for connection info");
      return NULL;
    }
    
    strncpy(info->ip, ip_buffer, 16);
    info->port = client_socket;

    pthread_t client_thread;
    pthread_create(&client_thread, NULL, handleClientConnection, (void *)info);
  }
}

char *operation_handler(char **tokens, int num_tokens)
{
  // Func to handle the operations

  if (num_tokens == 0)
  {
    return "Invalid input";
  }
  if (strcmp(tokens[0], "READ") == 0)
  {
    if(num_tokens != 2)
    {
      return "Invalid input";
    }
    // printf("READ\n");
    return read_or_getinfo_file(tokens[1]);
  } 
  else if (strcmp(tokens[0], "WRITE") == 0)
  {
    if(num_tokens != 3)
    {
      return "Invalid input";
    }
    return write_file(tokens[1]);
  }
  else if (strcmp(tokens[0], "DELETE") == 0)
  {
    if(num_tokens != 2)
    {
      return "Invalid input";
    }
    return delete_file(tokens[1]);
  }
  else if (strcmp(tokens[0], "LS") == 0)
  {
    if(num_tokens != 1)
    {
      return "Invalid input";
    }
    return list();
  }
  else if (strcmp(tokens[0], "INFO") == 0)
  {
    if(num_tokens != 2)
    {
      return "Invalid input";
    }
    read_or_getinfo_file(tokens[1]);
  }
  else if (strcmp(tokens[0], "STREAM")==0){
    if(num_tokens != 2)
    {
      return "Invalid input";
    }
    return read_or_getinfo_file(tokens[1]);
  }
  else if (strcmp(tokens[0], "CREATE") == 0)
  {
    if(num_tokens != 2)
    {
      return "Invalid input";
    }
    return create_file(tokens[1]);
  }
  else if(strcmp(tokens[0], "COPY") == 0)
  {
    if(num_tokens != 3)
    {
      return "Invalid input";
    }
    return copy_file(tokens[1], tokens[2]);
  }
  else
  {
    return "Invalid input";
  } 
}

char *read_or_getinfo_file(char *file){
  printf(GRN "File to be read: %s\n" reset, file);
  entry_info *entry;
  if((entry = getFromLRUCache(cache, file)) == NULL){
    printf(RED "File not found in cache\n" reset);
    return RED "File not found" reset;
  }
  printf(CYN "Incrementing reader count to %d for %s\n file" reset, entry->no_readers, file);
  sem_wait(&entry->read_mutex);
  entry->no_readers++;
  sem_post(&entry->read_mutex);

  char *response = (char *)malloc(1024 * sizeof(char));
  
  strcpy(response,"lookup response\nip: ");
  strcat(response, entry->ip);  
  strcat(response, "\n");
  strcat(response, "nm_port: ");
  char port[10];
  sprintf(port, "%d", entry->cl_port);
  strcat(response, port);


  // printf("response : %s\n", response);

  return response;

}

char *write_file(char *file){
  printf(GRN "File to be written: %s\n" reset, file);
  entry_info *entry;
  if((entry = getFromLRUCache(cache, file)) == NULL){
    printf(RED "File not found in cache\n" reset);
    return RED "File not found" reset;
  }
  
  if(entry->is_writing == 1){
    printf(RED "File is being written\n" reset);
    return RED "File is being written" reset;
  }

  printf(CYN "Setting isWriting to 1 for %s\n file" reset, file);
  sem_wait(&entry->write_mutex);
  entry->is_writing = 1;
  sem_post(&entry->write_mutex);


  char *response = (char *)malloc(1000 * sizeof(char));

  strcpy(response,"lookup response\nip: ");
  strcat(response, entry->ip);
  strcat(response, "\n");
  strcat(response, "nm_port: ");
  char port[10];
  sprintf(port, "%d", entry->cl_port);
  strcat(response, port);

  return response;
} 

char *delete_file(char *file){
  printf(GRN "File to be deleted: %s\n" reset, file);
  entry_info *entry;
  if((entry = getFromLRUCache(cache, file)) == NULL){
    printf(RED "File not found in cache\n" reset);
    return RED "File not found" reset;
  }
  
  int server_socket = connect_to_server(entry->ip, entry->nm_port);

  if (server_socket < 0)
  {
    printf(RED "Failed to connect to the storage server\n" reset);
    return RED "Failed to connect to the storage server" reset;
  }

  char *response = (char *)malloc(1024 * sizeof(char));
  strcpy(response, "DELETE ");
  strcat(response, file);
  printf(CYN "Sending delete request to storage server for %s file/folder\n" reset, file);
  send(server_socket, response, strlen(response), 0);

  char *response_from_server = (char *)malloc(1024 * sizeof(char));
  memset(response_from_server, 0, 1024);
  read(server_socket, response_from_server, 1023);
  close(server_socket);


  if(strcmp(response_from_server, "DELETED FOLDER") == 0){
    printf(GRN "DELETED %s folder\n" reset, file);
    removeFolderFromCache(cache, file);
    return GRN "DELETED FOLDER" reset;
  }
  else if(strcmp(response_from_server, "DELETED FILE") == 0){
    printf(GRN "DELETED %s file\n" reset, file);
    removeFileFromCache(cache, file);
    return GRN "DELETED FILE" reset;
  }
  else{
    printf(RED "Failed to delete %s file/folder\n" reset, file);
    return RED "Failed to delete file/folder" reset;
  }

  return GRN "file/folder deleted" reset;

}

int connect_to_server(char *ip, int port){
  if (ip == NULL || port < 0)
  {
    perror("Invalid IP or port");
    return -1;
  }
  
  int server_socket;
  struct sockaddr_in server_address, client_address;
  char server_ip[16];

  printf(CYN "Connecting to ss at %s:%d\n" reset, ip, port);
  printf(CYN "Creating socket via port %d\n" reset, NM_SS_PORT_CONNECT);

  client_address.sin_family = AF_INET;
  client_address.sin_port = htons(NM_SS_PORT_CONNECT);
  client_address.sin_addr.s_addr = htonl(INADDR_ANY);

  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0)
  {
    perror("server_socket creation failed");
    return -1;
  }
  bind(server_socket, (struct sockaddr *)&client_address, sizeof(client_address));

  memset(&server_address, '0', sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);

  if (inet_pton(AF_INET, ip, &server_address.sin_addr) <= 0)
  {
    perror("Invalid address/ Address not supported");
    return -1;
  }

  if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
  {
    perror("Connection Failed");
    return -1;
  }

  return server_socket;

}

char *list(){
  printf(GRN "List of all files/folders\n" reset);
  char *entries=get_all_entries(*cache->hashmap);

  char *response = (char *)malloc(1024 * sizeof(char));
  strcpy(response, GRN "List of all files/folders\n" reset);
  strcat(response, entries);
  printf("response : %s\n", response);
  return response;

}

entry_info *check_if_file_exists(char *file) {


  char *last_slash;
  char *folder = (char *)malloc(100 * sizeof(char));
  char *file_name = (char *)malloc(100 * sizeof(char));

  if (folder == NULL || file_name == NULL) {
    printf(RED "Malloc failed\n" reset);
    return NULL;
  }

  // Find the last occurrence of '/'
  last_slash = strrchr(file, '/');

  if (last_slash == NULL) {
    printf(RED "Invalid path\n" reset);
    return NULL;
  }

  // Check if the input ends in a '/'
  if (file[strlen(file) - 1] == '/') {
    // Remove the last '/'
    file[strlen(file) - 1] = '\0';
  }

  last_slash = strrchr(file, '/');

  if (last_slash == NULL) {
    printf(RED "Trying to make a new root folder\n" reset);
    return NULL;
  }

  int pos = last_slash - file;

  strncpy(folder, file, pos);
  folder[pos] = '/';
  folder[pos + 1] = '\0'; // Null-terminate the string

  strcpy(file_name, last_slash + 1);

  printf(GRN "Folder: %s\n" reset, folder);
  printf(GRN "File: %s\n" reset, file_name);

  entry_info *entry;
  if ((entry = getFromLRUCache(cache, folder)) == NULL) {
    printf(RED "Folder not found in cache\n" reset);
    return NULL;
  }

  return entry;
}


char *create_file(char *file){
  printf(GRN "File to be created: %s\n" reset, file);
  char temp_file[100];
  strcpy(temp_file, file);
  
  entry_info *entry;
  entry = check_if_file_exists(temp_file);
  printf("file : %s\n", file);
  if(entry == NULL){
    return RED "Invalid path" reset;
  }

  int server_socket = connect_to_server(entry->ip, entry->nm_port);
  if (server_socket < 0)
  {
    printf(RED "Failed to connect to the storage server\n" reset);
    return RED "Failed to connect to the storage server" reset;
  }
  char *response = (char *)malloc(1024 * sizeof(char));
  strcpy(response, "CREATE ");
  strcat(response, file);
  printf(CYN "Sending create request to storage server for %s file\n" reset, file);
  send(server_socket, response, strlen(response), 0);


  char *response_from_server = (char *)malloc(1024 * sizeof(char));
  memset(response_from_server, 0, 1024);
  read(server_socket, response_from_server, 1023);
  close(server_socket);

  if(strcmp(response_from_server, "CREATED") == 0){
    printf(GRN "CREATED %s file\n" reset, file);
    sem_t write_mutex, read_mutex;
    sem_init(&write_mutex, 0, 1);
    sem_init(&read_mutex, 0, 1);
    entry_info value;
    strcpy(value.ip, entry->ip);
    value.cl_port = entry->cl_port;
    value.nm_port = entry->nm_port;
    value.is_writing = 0;
    value.no_readers = 0;
    value.write_mutex = write_mutex;
    value.read_mutex = read_mutex;
    putInLRUCache(cache, file, value);

    return GRN "CREATED FILE" reset;

  }
  else{
    printf(RED "Failed to create %s file\n" reset, file);
    return RED "Failed to create file" reset;
  }


}


char *copy(char *src, char *dest){
  printf(GRN "copying file: %s to %s\n" reset, src, dest);

  entry_info *entry;
  if((entry = getFromLRUCache(cache, src)) == NULL){
    printf(RED "File not found in cache\n" reset);
    return RED "File not found" reset;
  }

  entry_info *entry_dest;
  if((entry_dest = getFromLRUCache(cache, dest)) == NULL){
    printf(RED "Destination folder not found in cache\n" reset);
    return RED "Destination folder not found" reset;
  }

  if(dest[strlen(dest)-1] != '/'){
    printf(RED "Invalid destination folder\n" reset);
    return RED "Invalid destination folder" reset;
  }

  if(src[strlen(src)-1] == '/'){
    printf("here at conditon\n");
    return copy_directory(src, dest);
  } 
  else{
    return copy_file(src, dest);
  }

  return GRN "file/folder copied" reset;

}

void sort_contents(char **contents, int num_contents)
{
    /*
        Sorts the contents of a folder in alphabetical order using Selection Sort
    */

    for (int i = 0; i < num_contents - 1; ++i)
    {
        int min_index = i;

        // Find the minimum element in the unsorted portion
        for (int j = i + 1; j < num_contents; ++j)
        {
            if (strcmp(contents[j], contents[min_index]) < 0)
            {
                min_index = j;
            }
        }

        // Swap the found minimum element with the first element
        if (min_index != i)
        {
            char *temp = contents[i];
            contents[i] = contents[min_index];
            contents[min_index] = temp;
        }
    }
}


char *copy_directory(char *src, char *dest){
  printf(GRN "copying directory: %s to %s\n" reset, src, dest);

  entry_info *entry;
  // entry = getFromLRUCache(cache, src);
  if((entry = getFromLRUCache(cache, src)) == NULL){
    printf(RED "Directory not found in cache\n" reset);
    return RED "Directory not found" reset;
  }
  char temp[1024];  

  entry_info *entry_dest;
  // entry_dest = getFromLRUCache(cache, dest);
  if((entry_dest = getFromLRUCache(cache, dest)) == NULL){
    printf(RED "Destination folder not found in cache\n" reset);
    return RED "Destination folder not found" reset;
  }

  int contents = 0;
  char **entries = get_folder_entries(*cache->hashmap, src);
  char **entries_dest = get_folder_entries_without_folder_name(*cache->hashmap, dest);

  int i = 0;
  while(entries[i] != NULL) {
    contents++;
    i++;
  }

  for (int i=0;entries_dest[i]!=NULL;i++){
    strcpy(temp, dest);
    strcat(temp, entries_dest[i]);
    strcpy(entries_dest[i], temp);
  }

  sort_contents(entries, contents);
  sort_contents(entries_dest, contents);

  for (int i=0; entries_dest[i]!=NULL; i++){
    if(entries_dest[i][strlen(entries_dest[i])-1] == '/'){
      create_dir_in_dest_server(entry_dest, entries_dest[i]);
    }
    else{
      create_file_in_dest_server(entry_dest, entries_dest[i]);
      read_and_save_file(entry, entries[i]);
      write_file_in_dest_server(entry_dest, entries_dest[i], entries[i]);
    }
  }

  return GRN "directory copied" reset;
}


char *create_dir_in_dest_server(entry_info *entry, char *dir){
  printf(GRN "Creating directory: %s\n" reset, dir);
  char *response_to_ss = (char *)malloc(1024 * sizeof(char));

  int server_socket = connect_to_server(entry->ip, entry->nm_port);
  if (server_socket < 0)
  {
    printf(RED "Failed to connect to the storage server\n" reset);
    return RED "Failed to connect to the storage server" reset;
  }

  strcpy(response_to_ss, "CREATEFOLDER ");
  strcat(response_to_ss, dir);
  printf(CYN "Sending create folder request to storage server for %s folder\n" reset, dir);
  send(server_socket, response_to_ss, strlen(response_to_ss), 0);

  char *response_from_server = (char *)malloc(1024 * sizeof(char));
  memset(response_from_server, 0, 1024);
  read(server_socket, response_from_server, 1023);
  close(server_socket);

  if(strcmp(response_from_server, "CREATED") == 0){
    printf(GRN "CREATED %s folder\n" reset, dir);
    sem_t write_mutex, read_mutex;
    sem_init(&write_mutex, 0, 1);
    sem_init(&read_mutex, 0, 1);
    entry_info value;
    strcpy(value.ip, entry->ip);
    value.cl_port = entry->cl_port;
    value.nm_port = entry->nm_port;
    value.is_writing = 0;
    value.no_readers = 0;
    value.write_mutex = write_mutex;
    value.read_mutex = read_mutex;
    putInLRUCache(cache, dir, value);

    return GRN "CREATED FOLDER" reset;

  }
  else{
    printf(RED "Failed to create %s folder\n" reset, dir);
    return RED "Failed to create folder" reset;
  }

}

// char *create_file_in_dest_server(entry_info *entry, char *file){
//   printf(GRN "Creating file %s in %d\n" reset, file, entry->nm_port);

//   char *response_to_ss = (char *)malloc(1024 * sizeof(char));

//   int server_socket = connect_to_server(entry->ip, entry->nm_port);
//   strcpy(response_to_ss, "CREATE ");
//   strcat(response_to_ss, file);
//   printf(CYN "Sending create request to storage server for %s file\n" reset, file);
//   send(server_socket, response_to_ss, strlen(response_to_ss), 0);

//   char *response_from_server = (char *)malloc(1024 * sizeof(char));
//   memset(response_from_server, 0, 1024);
//   read(server_socket, response_from_server, 1023);
//   close(server_socket);


//   if(strcmp(response_from_server, "CREATED") == 0){
//     printf(GRN "CREATED %s file\n" reset, file);
//     sem_t write_mutex, read_mutex;
//     sem_init(&write_mutex, 0, 1);
//     sem_init(&read_mutex, 0, 1);
//     entry_info value;
//     strcpy(value.ip, entry->ip);
//     value.cl_port = entry->cl_port;
//     value.nm_port = entry->nm_port;
//     value.is_writing = 0;
//     value.no_readers = 0;
//     value.write_mutex = write_mutex;
//     value.read_mutex = read_mutex;
//     putInLRUCache(cache, file, value);

//     return GRN "CREATED FILE" reset;

//   }
//   else{
//     printf(RED "Failed to create %s file\n" reset, file);
//     return RED "Failed to create file" reset;
//   }

// }

char *create_file_in_dest_server(entry_info *entry, char *file) {
    printf(GRN "Creating file %s in %d\n" reset, file, entry->nm_port);

    char *response_to_ss = (char *)malloc(1024 * sizeof(char));
    if (response_to_ss == NULL) {
        perror("Memory allocation failed");
        return RED "Memory allocation failed" reset;
    }

    int server_socket = connect_to_server(entry->ip, entry->nm_port);
    if (server_socket < 0) {
        free(response_to_ss);
        return RED "Failed to connect to the storage server" reset;
    }

    snprintf(response_to_ss, 1024, "CREATE %s", file);
    printf(CYN "Sending create request to storage server for %s file\n" reset, file);
    send(server_socket, response_to_ss, strlen(response_to_ss), 0);

    char *response_from_server = (char *)malloc(1024 * sizeof(char));
    if (response_from_server == NULL) {
        perror("Memory allocation failed");
        free(response_to_ss);
        close(server_socket);
        return RED "Memory allocation failed" reset;
    }
    memset(response_from_server, 0, 1024);
    read(server_socket, response_from_server, 1023);
    close(server_socket);

    if (strcmp(response_from_server, "CREATED") == 0) {
        printf(GRN "CREATED %s file\n" reset, file);
    } else {
        printf(RED "Failed to create %s file\n" reset, file);
    }

    free(response_to_ss);
    free(response_from_server);
    return response_from_server;
}

char *read_and_save_file(entry_info *entry, char *file){
  // printf(GRN "Reading file %s from %d\n" reset, file, entry->nm_port);

  char *file_path=(char *)malloc(100 * sizeof(char));
  char temp_src_path[100];

  char *response_to_ss = (char *)malloc(1024 * sizeof(char));

  strcpy(temp_src_path, file);

  char *last_slash = strrchr(temp_src_path, '/');

  if(last_slash != NULL){
    file_path = strchr(temp_src_path, '/');
    if (file_path != NULL) {
        file_path++;
    }
  }
  else{
    strcpy(file_path, file);
  }

  strcat(file_path, "_temp");

  FILE *fp = fopen(file_path, "w");

  printf("created a temp file in NM\n");

  strcpy(response_to_ss, "READ ");

  strcat(response_to_ss, file);
  printf(CYN "Sending read request to storage server for %s file\n" reset, file);

  int server_socket = connect_to_server(entry->ip, entry->nm_port);
  if (server_socket < 0)
  {
    printf(RED "Failed to connect to the storage server\n" reset);
    return RED "Failed to connect to the storage server" reset;
  }
  send(server_socket, response_to_ss, strlen(response_to_ss), 0);
  char* response_from_server = (char *)malloc(1024 * sizeof(char));
  memset(response_from_server, 0, 1024);
  read(server_socket, response_from_server, 1023);
  close(server_socket);


  printf(GRN "File content received from storage server\n" reset);
  fprintf(fp, "%s", response_from_server);
  fclose(fp);

  return "NM has created a file in destination server";

} 

// char *write_file_in_dest_server(entry_info *entry, char *dest, char *src){
  
//   char *filename=(char *)malloc(100 * sizeof(char));
//   char temp_src_path[100];

//   char *response_to_ss = (char *)malloc(1024 * sizeof(char));

//   strcpy(response_to_ss, "NMWRITE ");
//   strcat(response_to_ss, dest);
//   strcat(response_to_ss, " ");


//   strcpy(temp_src_path, src);

//   char *last_slash = strrchr(temp_src_path, '/');
//   if(last_slash != NULL){
//     filename = strchr(temp_src_path, '/');
//     if (filename != NULL) {
//         filename++;
//     }
//   }
//   else{
//     strcpy(filename, src);
//   }

//   strcat(filename, "_temp");

//   FILE *fp = fopen(filename, "r");
//   if (fp==NULL){
//     printf(RED "File not found\n" reset);
//     return RED "File not found" reset;
//   }
//   printf("opened the temp file in NM\n");

//   char c;
//   char *content = (char *)malloc(1024 * sizeof(char));
//   int i = 0;
//   while((c = fgetc(fp)) != EOF){
//     if(strlen(response_to_ss) + strlen(content) >= 1024){
//       // response_to_ss = (char *)realloc(response_to_ss, 2 * strlen(response_to_ss) * sizeof(char));
//       printf(RED "file too big\n" reset);
//     }
//     content[i] = c;
//     i++;
//   }
//   content[i] = '\0';

//   strcat(response_to_ss, content);
//   fclose(fp);

//   printf(CYN "Sending write request to storage server for %s file\n" reset, dest);
//   printf("content: %s\n", content);

//   int server_socket = connect_to_server(entry->ip, entry->nm_port);
//   if (server_socket < 0)
//   {
//     printf(RED "Failed to connect to the storage server\n" reset);
//     return RED "Failed to connect to the storage server" reset;
//   }
//   send(server_socket, response_to_ss, strlen(response_to_ss), 0);
//   char* response_from_server = (char *)malloc(1024 * sizeof(char));
//   memset(response_from_server, 0, 1024);
//   read(server_socket, response_from_server, 1023);
//   close(server_socket);

//   remove(filename);

//   if (strcmp(response_from_server, "WRITTEN") == 0){
//     printf(GRN "File written to %s\n" reset, dest);
//     return GRN "File written" reset;
//   }
//   else{
//     printf(RED "Failed to write file to %s\n" reset, dest);
//     return RED "Failed to write file" reset;
//   }

// }
char *write_file_in_dest_server(entry_info *entry, char *dest, char *src) {
    char *filename = (char *)malloc(100 * sizeof(char));
    if (filename == NULL) {
        perror("Memory allocation failed");
        return RED "Memory allocation failed" reset;
    }
    char temp_src_path[100];

    char *response_to_ss = (char *)malloc(1024 * sizeof(char));
    if (response_to_ss == NULL) {
        perror("Memory allocation failed");
        free(filename);
        return RED "Memory allocation failed" reset;
    }

    snprintf(response_to_ss, 1024, "NMWRITE %s ", dest);

    strcpy(temp_src_path, src);
    char *last_slash = strrchr(temp_src_path, '/');
    if (last_slash != NULL) {
        strcpy(filename, last_slash + 1);
    } else {
        strcpy(filename, temp_src_path);
    }
    strcat(filename, "_temp");

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Failed to open temp file");
        free(filename);
        free(response_to_ss);
        return RED "Failed to open temp file" reset;
    }
    printf("Opened the temp file in NM\n");

    char c;
    char *content = (char *)malloc(1024 * sizeof(char));
    if (content == NULL) {
        perror("Memory allocation failed");
        fclose(fp);
        free(filename);
        free(response_to_ss);
        return RED "Memory allocation failed" reset;
    }
    int i = 0;
    while ((c = fgetc(fp)) != EOF) {
        content[i++] = c;
    }
    content[i] = '\0';

    strcat(response_to_ss, content);
    fclose(fp);

    printf(CYN "Sending write request to storage server for %s file\n" reset, dest);
    printf("Content: %s\n", content);

    int server_socket = connect_to_server(entry->ip, entry->nm_port);
    if (server_socket < 0) {
        free(filename);
        free(response_to_ss);
        free(content);
        return RED "Failed to connect to the storage server" reset;
    }
    send(server_socket, response_to_ss, strlen(response_to_ss), 0);

    char *response_from_server = (char *)malloc(1024 * sizeof(char));
    if (response_from_server == NULL) {
        perror("Memory allocation failed");
        free(filename);
        free(response_to_ss);
        free(content);
        close(server_socket);
        return RED "Memory allocation failed" reset;
    }
    memset(response_from_server, 0, 1024);
    read(server_socket, response_from_server, 1023);
    close(server_socket);

    remove(filename);

    if (strcmp(response_from_server, "WRITTEN") == 0) {
        printf(GRN "Written %s file\n" reset, dest);
    } else {
        printf(RED "Failed to write %s file\n" reset, dest);
    }

    free(filename);
    free(response_to_ss);
    free(content);
    return response_from_server;
}

  

char *copy_file(char *src, char *dest){
  printf(GRN "copying file: %s to %s\n" reset, src, dest);

  entry_info *entry_src;
  entry_info *entry_dest;
  char *temp_src_path = (char *)malloc(100 * sizeof(char));
  char *dest_path = (char *)malloc(100 * sizeof(char));
  char *filename = (char *)malloc(100 * sizeof(char));

  strcpy(temp_src_path, src);
  strcpy(dest_path, dest);

  char *last_slash = strrchr(temp_src_path, '/');
  if(last_slash != NULL){
    filename = strchr(temp_src_path, '/');
    if (filename != NULL) {
        filename++;
    }
  }
  else{
    strcpy(filename, src);
  }

  strcat(dest_path, filename);

  if((entry_src = getFromLRUCache(cache, src)) == NULL){
    printf(RED "File not found in cache\n" reset);
    return RED "File not found" reset;
  }
  if((entry_dest = getFromLRUCache(cache, dest)) == NULL){
    printf(RED "Destination folder not found in cache\n" reset);
    return RED "Destination folder not found" reset;
  }

  create_file_in_dest_server(entry_dest, dest_path);
  printf("created file in destination server\n");

  read_and_save_file(entry_src, src);
  printf("read file from source server\n");

  write_file_in_dest_server(entry_dest, dest_path, src);
  printf("wrote file to destination server\n");

  return GRN "file copied" reset;

}
 
void backup_init() {
    printf("Starting to create the backups now.\n");

    ListNode *node = head_list;
    while (node != NULL) {
        char *ip = node->ip;
        int nm_port = node->nm_port;
        int client_port = node->client_port;

        printf("Finding all accessible paths for SS %s:%d\n", ip, nm_port);
        char **paths = find_by_ip(*cache->hashmap, ip);
        int num_paths = 0;
        while (paths[num_paths] != NULL) {
            num_paths++;
        }

        ListNode *bkp_1_SS = head_list;
        ListNode *bkp_2_SS = head_list->next;

        printf("Found backup server 1! (%s:%d)\n", bkp_1_SS->ip, bkp_1_SS->nm_port);
        printf("Found backup server 2! (%s:%d)\n", bkp_2_SS->ip, bkp_2_SS->nm_port);

        printf("Saving the backup for %s:%d in: %s:%d and %s:%d\n", ip, nm_port, bkp_1_SS->ip, bkp_1_SS->nm_port, bkp_2_SS->ip, bkp_2_SS->nm_port);

        for (int i = 0; i < num_paths; i++) {
            char *src_path = paths[i];
            char dest_path[1024];
            snprintf(dest_path, sizeof(dest_path), "bkps/%s", src_path);

            printf("Copying path %s\n", src_path);

            // Create entry_info for backup server 1
            entry_info bkp_1;
            strncpy(bkp_1.ip, bkp_1_SS->ip, sizeof(bkp_1.ip) - 1);
            bkp_1.ip[sizeof(bkp_1.ip) - 1] = '\0';
            bkp_1.nm_port = bkp_1_SS->nm_port;
            bkp_1.cl_port = bkp_1_SS->client_port;
            bkp_1.is_writing = 0;
            bkp_1.no_readers = 0;
            sem_init(&bkp_1.write_mutex, 0, 1);
            sem_init(&bkp_1.read_mutex, 0, 1);

            // Create file in backup server 1
            printf("Creating file %s in %d\n", dest_path, bkp_1.nm_port);
            create_file_in_dest_server(&bkp_1, dest_path);

            // Read and save file from source server
            entry_info src_entry;
            strncpy(src_entry.ip, node->ip, sizeof(src_entry.ip) - 1);
            src_entry.ip[sizeof(src_entry.ip) - 1] = '\0';
            src_entry.nm_port = node->nm_port;
            src_entry.cl_port = node->client_port;
            src_entry.is_writing = 0;
            src_entry.no_readers = 0;
            sem_init(&src_entry.write_mutex, 0, 1);
            sem_init(&src_entry.read_mutex, 0, 1);
            read_and_save_file(&src_entry, src_path);

            // Write file to backup server 1
            write_file_in_dest_server(&bkp_1, dest_path, src_path);

            // Create entry_info for backup server 2
            entry_info bkp_2;
            strncpy(bkp_2.ip, bkp_2_SS->ip, sizeof(bkp_2.ip) - 1);
            bkp_2.ip[sizeof(bkp_2.ip) - 1] = '\0';
            bkp_2.nm_port = bkp_2_SS->nm_port;
            bkp_2.cl_port = bkp_2_SS->client_port;
            bkp_2.is_writing = 0;
            bkp_2.no_readers = 0;
            sem_init(&bkp_2.write_mutex, 0, 1);
            sem_init(&bkp_2.read_mutex, 0, 1);

            // Create file in backup server 2
            printf("Creating file %s in %d\n", dest_path, bkp_2.nm_port);
            create_file_in_dest_server(&bkp_2, dest_path);

            // Write file to backup server 2
            write_file_in_dest_server(&bkp_2, dest_path, src_path);
        }

        node = node->next;
    }

    printf("Backup initialization done\n");
}

#endif