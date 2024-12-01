#ifndef LRU_CACHING_H
#define LRU_CACHING_H

#include "misc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CACHE_SIZE 5
#define HASH_MAP_SIZE 1000

typedef struct cacheNode {
    struct cacheNode *prev, *next;
    char key[256];
    time_t lastAccessed; // Timestamp of the last access
    int refCount; // Number of references to this node
    int isDirty; // Flag to indicate if the node has been modified
    entry_info value;
} cacheNode;

typedef struct LRUCache{
    
    hash_table_entry *hashmap[MAX_HASH_TABLE_SIZE];
    cacheNode *Nodes[CACHE_SIZE];
    int size;
    cacheNode *head, *tail;
}LRUCache;

  
static LRUCache *cache;

cacheNode *createCacheNode(const char *key, entry_info value);
cacheNode *createCacheNode(const char *key, entry_info value)
{
   
    cacheNode *node;
    if (!(node = (cacheNode *)malloc(sizeof(*node))))
    {
        fprintf(stderr, "Failed to allocate memory for cache node.\n");
        return NULL;
    }

    // Copy the key with a safer method
    if (snprintf(node->key, sizeof(node->key), "%s", key) >= sizeof(node->key))
    {
        fprintf(stderr, "Key is too long to fit in cache node.\n");
        free(node);
        return NULL;
    }

    // Initialize the value
    node->value = value;

    // Initialize pointers to NULL
    node->prev = NULL;
    node->next = NULL;
    node->lastAccessed = time(NULL);
    node->refCount = 1;
    node->isDirty = 0;
    return node;
}

void detachCacheNode(LRUCache *cache, cacheNode *node);
void detachCacheNode(LRUCache *cache, cacheNode *node)
{  
    if (node == NULL){
        return;
    }
    // If the node is at the head, update the head pointer
    cache->head = (cache->head == node) ? node->next : cache->head;
    
    // Handle tail pointer update
    cache->tail = (cache->tail == node) ? node->prev : cache->tail;
    
    // Update adjacent nodes' links
    if (node->next) node->next->prev = node->prev;
    if (node->prev) node->prev->next = node->next;
    return ;
}
void attachCacheNodeAtFront(LRUCache *cache, cacheNode *node);
void attachCacheNodeAtFront(LRUCache *cache, cacheNode *node)
{
    if (node == NULL)  // Check if the node is NULL
    {
        return ;  // Return error if node is NULL
    }
    int a =0;
    if(node->prev == NULL){
        a = 1;
    }
    int b=0;
    if(node->next == NULL){
        b=1;
    }
    if(a==0 || b ==0){
        // If the node is at the head, update the head pointer
        cache->head = (cache->head == node) ? node->next : cache->head;
        
        // Handle tail pointer update
        cache->tail = (cache->tail == node) ? node->prev : cache->tail;
        
        // Update adjacent nodes' links
        if (node->next) node->next->prev = node->prev;
        if (node->prev) node->prev->next = node->next;
    }
    //Now attach to the front
    node->next = cache->head;
    node->prev = NULL;
    if(cache->head== NULL ){
        cache->head = node;
        if(!cache->tail){
            cache->tail = node;
        }
        
    }else{
         cache->head->prev = node;
         cache->head = node;
         if(cache->tail){
             //do nothing maybe
         }
         else{
            cache->tail = node;
         }
    }
    return;
}
void removeFolderFromCache(LRUCache *cache, const char *folder);
void removeFolderFromCache(LRUCache *cache, const char *folder)
{
   size_t folder_len = strlen(folder);
   cacheNode *current_node = cache->head;
   while(current_node != NULL){
       cacheNode *node = current_node;
        if (strncmp(node->key, folder, folder_len) != 0) {
            current_node= current_node->next;
            continue;
        }
        else if(strncmp(node->key, folder, folder_len) == 0){
                cache->head = (cache->head == node) ? node->next : cache->head;
                cache->tail = (cache->tail == node) ? node->prev : cache->tail;
                if (node->next) node->next->prev = node->prev;
                if (node->prev) node->prev->next = node->next;
                cache->size--;
                current_node = current_node->next;
                free(node);
        }
   }
   delete_folder(cache->hashmap, folder);
      
}
void removeFileFromCache(LRUCache *cache, const char *fileKey);
void removeFileFromCache(LRUCache *cache, const char *fileKey){
    if (!cache || !cache->head || !fileKey) {
        return;
    }
    cacheNode *current_node = cache->head;
    cacheNode *to_Remove = NULL;
    int node_is_null=0;
    while(!node_is_null){
        if(strcmp(current_node->key, fileKey) == 0){
            to_Remove = current_node;
            break;
        }
        if(current_node->next == NULL){
            node_is_null = 1;
        }
        current_node = current_node->next;
    }
    if (to_Remove != NULL) {
    cache->head = (to_Remove->prev) ? cache->head : to_Remove->next;
    cache->tail = (to_Remove->next) ? cache->tail : to_Remove->prev;
    
    if (to_Remove->prev) to_Remove->prev->next = to_Remove->next;
    if (to_Remove->next) to_Remove->next->prev = to_Remove->prev;
    
        free(to_Remove);
        cache->size--;
    }
    remove_key(*cache->hashmap, fileKey);
    return;
}
bool isSpaceAvailable(LRUCache *cache) {
    return cache->size < CACHE_SIZE;
}

void putInLRUCache(LRUCache *cache, const char *key, entry_info value);
void putInLRUCache(LRUCache *cache, const char *key, entry_info value)
{
   cacheNode* node = NULL;
   int alreadyInCache = 0;
   int i =0 ;
   while(i<cache->size){
      if(cache->Nodes[i]!=NULL ){
            if(strcmp(cache->Nodes[i]->key, key) == 0){
            node = cache->Nodes[i];
            alreadyInCache = 1;
            break;
            }
       }
       i++;
    }
    if(alreadyInCache == 1){
        node->value = value;
        node->isDirty = 1;
        node->lastAccessed = time(NULL);
        node->refCount++;
        attachCacheNodeAtFront(cache, node);
    }
    else{
        if(cache->size >= CACHE_SIZE){
          cacheNode *node = cache->tail;
           detachCacheNode(cache, node);
            int index = 0;
            while(index < cache->size){
                if(cache->Nodes[index] == node){
                    int i = index;
                    while(i < cache->size){
                        cache->Nodes[i] = cache->Nodes[i+1];
                        i++;
                    }
                    break;
                }
                index++;
            }
            cache->size--;
            free(node);
        }
        cacheNode* new_mem = createCacheNode(key, value);
        attachCacheNodeAtFront(cache, new_mem);
        isSpaceAvailable(cache) ? cache->Nodes[cache->size++] = new_mem : 0;
        bool shouldInsert = (search_hash_table(*cache->hashmap, key) == NULL);
        if (shouldInsert) {
            insert_hash_table(cache->hashmap, key, value);
        }
    }

}
entry_info *getFromLRUCache(LRUCache *cache, const char *key);
entry_info* if_value(LRUCache* cache, const char* key,entry_info* value){
    if(value==NULL){
        return NULL;
    }
    else{
        printf("File not found in cache\n");
        putInLRUCache(cache, key, *value);
        return value;
    }
}
entry_info *getFromLRUCache(LRUCache *cache, const char *key)
{
    int i = 0;
    while(i<cache->size){
        if(cache->Nodes[i]!=NULL){
            if(strcmp(cache->Nodes[i]->key, key) == 0){
                printf("File found in cache\n");
                cacheNode *node = cache->Nodes[i];
                if(cache!=NULL && node!=NULL){
                    detachCacheNode(cache, node);
                    attachCacheNodeAtFront(cache, node);
                }
                return &node->value;
            }
        }
        i++;
    }
    printf("File not found in cache\n");
    entry_info *value = find_hash_table(*cache->hashmap, key);
    printf("File not found in hash_table\n");
    printf("value : %s\n", value->ip);
    return if_value(cache, key, value);
}

void initialise_LRUcache(LRUCache *cache) {
    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;
    // for (int i = 0; i < HASH_MAP_SIZE; ++i) {
    //     cache->hashmap[i] = (hash_table_entry *)malloc(sizeof(hash_table_entry));
    //     if (cache->hashmap[i] == NULL) {
    //         perror("Failed to allocate memory for hash table entry");
    //         exit(1);
    //     }
    //     cache->hashmap[i]->next = NULL; // Initialize the next pointer to NULL
    // }
    init_hash_table(cache->hashmap);
}
void printt_value_data(entry_info *value);
void printf_value_data(entry_info *value){
    printf("ip : %s\n", value->ip);
    printf("nm_port : %d\n", value->nm_port);
    printf("client_port : %d\n", value->cl_port);
    printf("num_readers : %d\n", value->no_readers);
    printf("isWriting : %d\n", value->is_writing);
}
void printLRUcache(const LRUCache *cache);
void printLRUcache(const LRUCache *cache)
{
    cacheNode *node = cache->head;
    printf("LRU Cache Contents:\n");
    while (node != NULL)
    {
        printf("key : %s  \n", node->key);
        printf_value_data(&node->value);
        node = node->next;
    }
}

#endif // LRU_CACHING_H
