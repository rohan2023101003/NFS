#ifndef MISC_H
#define MISC_H

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



#define GRN "\e[0;32m"
#define MAG "\e[0;35m"
#define RED "\e[0;31m"
#define CYN "\e[0;36m"
#define reset "\e[0m"

#define NM_Client_PORT 8080     // port for naming server's communication with the client
#define NM_SS_PORT_LISTEN 8081  // naming server listens to this port for storage server's connection
#define NM_SS_PORT_CONNECT 8082 // naming server connects to storage servers through this port (only when NM is initiating the connection)
// #define BUFFER_RECV_SIZE 4096 

#define MAX_HASH_TABLE_SIZE 100 // maximum size of the hash table



// structure to store the information in hash table
typedef struct {
    char ip[16];
    int nm_port;
    int cl_port;
    int is_writing;
    int no_readers;
    sem_t write_mutex;
    sem_t read_mutex;
} entry_info;

typedef struct hash_table_entry {
    char key[100];
    entry_info value;
    struct hash_table_entry *next;
} hash_table_entry;
// Define the Node structure
typedef struct ListNode
{
    char ip[16]; // To store the IP address
    int nm_port; // To store the Port number
    int client_port;
    struct ListNode *next;
} ListNode;


static hash_table_entry *accessible_paths_hash_table[MAX_HASH_TABLE_SIZE];
static hash_table_entry *bkp_accessible_paths_hash_table[MAX_HASH_TABLE_SIZE];
ListNode* head_list = NULL; // head of the list of storage servers



unsigned int hash(const char *key) {
    unsigned int hash = 0;
    for (int i = 0; key[i] != '\0'; i++) {
        hash += key[i] + hash * 67;
    }
    return hash % MAX_HASH_TABLE_SIZE;
}

// function to initialize the hash table
void init_hash_table(hash_table_entry **hash_table) {
    *hash_table = (hash_table_entry *)malloc(MAX_HASH_TABLE_SIZE * sizeof(hash_table_entry));
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        (*hash_table)[i].key[0] = '\0';
        (*hash_table)[i].value.ip[0] = '\0';
        (*hash_table)[i].value.nm_port = 0;
        (*hash_table)[i].value.cl_port = 0;
        (*hash_table)[i].value.is_writing = 0;
        (*hash_table)[i].value.no_readers = 0;
        sem_init(&(*hash_table)[i].value.write_mutex, 0, 1);
        sem_init(&(*hash_table)[i].value.read_mutex, 0, 1);
        (*hash_table)[i].next = NULL;
    }
}

// function to insert an entry in the hash table
void insert_hash_table(hash_table_entry **hash_table, const char *key, entry_info value) {
    unsigned int index = hash(key);
    hash_table_entry *entry = &(*hash_table)[index];
    while (entry->next != NULL) {
        entry = entry->next;
    }
    entry->next = (hash_table_entry *)malloc(sizeof(hash_table_entry));
    entry = entry->next;
    strcpy(entry->key, key);
    entry->value = value;
    entry->next = NULL;
}

// func to find an entry in the hash table
entry_info *find_hash_table(hash_table_entry *hash_table, const char *key) {
    unsigned int index = hash(key);
    hash_table_entry *entry = &hash_table[index];
    while (entry->next != NULL) {
        entry = entry->next;
        if (strcmp(entry->key, key) == 0) {
            return &entry->value;
        }
    }
    
    return NULL;
}

// func to clean the hash table
void clean_hash_table(hash_table_entry **hash_table) {
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        hash_table_entry *entry = &(*hash_table)[i];
        while (entry->next != NULL) {
            hash_table_entry *temp = entry->next;
            entry->next = temp->next;
            free(temp);
        }
    }
    free(*hash_table);
}

// function to search for an entry in the hash table
hash_table_entry *search_hash_table(hash_table_entry *hash_table, const char *key) {
    unsigned int index = hash(key);
    hash_table_entry *entry = &hash_table[index];
    while (entry->next != NULL) {
        entry = entry->next;
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
    }
    return NULL;
}

// function to delete an entry from the hash table
void delete_hash_table(hash_table_entry **hash_table, char *key) {
    unsigned int index = hash(key);
    hash_table_entry *entry = &(*hash_table)[index];
    while (entry->next != NULL) {
        if (strcmp(entry->next->key, key) == 0) {
            hash_table_entry *temp = entry->next;
            entry->next = temp->next;
            free(temp);
            return;
        }
        entry = entry->next;
    }
}

//func to delete a key from the hash table
void remove_key(hash_table_entry *hash_table, const char *key){
    unsigned int index = hash(key);
    hash_table_entry *entry = &hash_table[index];
    while (entry->next != NULL) {
        if (strcmp(entry->next->key, key) == 0) {
            hash_table_entry *temp = entry->next;
            entry->next = temp->next;
            free(temp);
            return;
        }
        entry = entry->next;
    }
}



// func to delete a folder and all its contents from the hash table
void delete_folder(hash_table_entry **hash_table, const char *folder) {
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        hash_table_entry *entry = &(*hash_table)[i];
        while (entry->next != NULL) {
            if (strncmp(entry->next->key, folder, strlen(folder)) == 0) {
                hash_table_entry *temp = entry->next;
                entry->next = temp->next;
                free(temp);
            } else {
                entry = entry->next;
            }
        }
    }
}

// func to get all the entries in the hash table
char *get_all_entries(hash_table_entry *hash_table) {
    char *entries = (char *)malloc(10000 * sizeof(char));
    entries[0] = '\0';
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        hash_table_entry *entry = &hash_table[i];
        while (entry->next != NULL) {
            entry = entry->next;
            char temp[1000];
            sprintf(temp, "%s\n", entry->key);
            strcat(entries, temp);
        }
    }
    return entries;
}

// func for getting entries of a folder
char **get_folder_entries(hash_table_entry *hash_table, char *folder) {
    char **entries = (char **)malloc(100 * sizeof(char *));
    for (int i = 0; i < 1000; i++) {
        entries[i] = (char *)malloc(1000 * sizeof(char));
    }
    int count = 0;
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        hash_table_entry *entry = &hash_table[i];
        while (entry->next != NULL) {
            entry = entry->next;
            if (strncmp(entry->key, folder, strlen(folder)) == 0) {
                sprintf(entries[count], "%s\n", entry->key);
                count++;
            }
        }
    }
    return entries;
}

// func to get entries of a folder without folder name like 
// example: if folder is /a/b/c then entries will be of the form /a/b/c/1.txt then it returns like 1.txt
char **get_folder_entries_without_folder_name(hash_table_entry *hash_table, char *folder) {
    char **entries = (char **)malloc(100 * sizeof(char *));
    for (int i = 0; i < 1000; i++) {
        entries[i] = (char *)malloc(1000 * sizeof(char));
    }
    int count = 0;
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        hash_table_entry *entry = &hash_table[i];
        while (entry->next != NULL) {
            entry = entry->next;
            if (strncmp(entry->key, folder, strlen(folder)) == 0) {
                sprintf(entries[count], "%s\n", entry->key + strlen(folder));
                count++;
            }
        }
    }
    return entries;
}

// func to find all entries by ip because to delete the entries when a storage server is disconnected
char **find_by_ip(hash_table_entry *hash_table, char *ip) {
    char **entries = (char **)malloc(1000 * sizeof(char *));
    for (int i = 0; i < 1000; i++) {
        entries[i] = (char *)malloc(1000 * sizeof(char));
    }
    int count = 0;
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        hash_table_entry *entry = &hash_table[i];
        while (entry->next != NULL) {
            entry = entry->next;
            if (strcmp(entry->value.ip, ip) == 0) {
                sprintf(entries[count], "%s %s %d %d %d %d\n", entry->key, entry->value.ip, entry->value.nm_port, entry->value.cl_port, entry->value.is_writing, entry->value.no_readers);
                count++;
            }
        }
    }
    return entries;
}

// func to delete all entries by ip
void delete_by_ip(hash_table_entry **hash_table, char *ip) {
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        hash_table_entry *entry = &(*hash_table)[i];
        while (entry->next != NULL) {
            if (strcmp(entry->next->value.ip, ip) == 0) {
                hash_table_entry *temp = entry->next;
                entry->next = temp->next;
                free(temp);
            } else {
                entry = entry->next;
            }
        }
    }
}

void print_hash_table(hash_table_entry *hash_table) {
    for (int i = 0; i < MAX_HASH_TABLE_SIZE; i++) {
        hash_table_entry *entry = &hash_table[i];
        while (entry->next != NULL) {
            entry = entry->next;
            printf("%s %s %d %d %d %d\n", entry->key, entry->value.ip, entry->value.nm_port, entry->value.cl_port, entry->value.is_writing, entry->value.no_readers);
        }
    }
}



// Function to create a new node
ListNode *createListNode(const char *ip, int nm_port, int client_port)
{
    ListNode *newNode = (ListNode *)malloc(sizeof(ListNode));
    if (newNode == NULL)
    {
        printf("Memory allocation failed\n");
        return NULL;
    }
    strncpy(newNode->ip, ip, 15);
    newNode->ip[15] = '\0'; // Ensuring string termination
    newNode->nm_port = nm_port;
    newNode->client_port = client_port;
    newNode->next = NULL;
    return newNode;
}

// Function to insert a node at the beginning of the list
void insertNode(ListNode **head, const char *ip, int nm_port, int client_port)
{
    ListNode *newNode = createListNode(ip, nm_port, client_port);
    newNode->next = *head;
    *head = newNode;
}

// Function to delete a node by IP and port
void deleteNode(ListNode **head, const char *ip, int nm_port)
{
    ListNode *temp = *head, *prev = NULL;

    // If head node itself holds the key
    if (temp != NULL && strcmp(temp->ip, ip) == 0 && temp->nm_port == nm_port)
    {
        *head = temp->next;
        free(temp);
        return;
    }

    // Search for the key to be deleted
    while (temp != NULL && !(strcmp(temp->ip, ip) == 0 && temp->nm_port == nm_port))
    {
        prev = temp;
        temp = temp->next;
    }

    // If key was not present in linked list
    if (temp == NULL)
        return;

    // Unlink the node from linked list
    prev->next = temp->next;
    free(temp);
}

// Function to display the list
void displayList(ListNode *node)
{
    while (node != NULL)
    {
        printf("IP: %s, Ports: %d %d\n", node->ip, node->nm_port, node->client_port);
        node = node->next;
    }
}

#endif