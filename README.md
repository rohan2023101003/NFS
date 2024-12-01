# Network File System (NFS) - Distributed File System

## Introduction

This project implements a Network File System (NFS), also known as a Distributed File System. The NFS allows clients to interact with files stored across multiple storage servers through a central naming server. The major components of the system are:

- **Clients**: Systems or users requesting access to files within the NFS.
- **Naming Server (NM)**: A central hub that orchestrates communication between clients and storage servers.
- **Storage Servers (SS)**: Servers responsible for the physical storage and retrieval of files and folders.

## Assumptions

### Storage Server (SS)
1. The IP and Port of the Naming Server are known to all storage servers.
2. Only text files (e.g., .txt, .c, .cpp, .py) are stored.
3. No folder should end with a dot (.*).
4. All file and folder names are unique.
5. No files or folders can be created inside the root folder (base folder).
6. All files and folders are assumed to have permissions (0777).
7. TCP is used for all connections.
8. If an SS is disconnected, it still shows its files to clients but cannot be modified.
9. If there are more than two storage servers, backups are created.
10. Folders always end with '/' to differentiate between files and folders.
11. All files displayed in the accessible paths are only for read, write, delete, and copy.
12. Ports are assigned to NM and clients starting from 7001, incrementing by 1. If a port is busy, the next available port is assigned.

### Client
1. Commands cannot have spaces in front.
2. The data to be read or written should be less than the size of the data packet being sent.

### Naming Server (NM)
1. The NM manages the directory structure and maintains information about file locations.
2. The NM provides timely feedback to clients upon task completion.
3. The NM dynamically updates the list of accessible paths and backups.

## How to Run

This project uses a Makefile to manage the build and execution of the Naming Server, Storage Server, and Client. To run the project, use the following commands:

1. **Compile and Run the Naming Server**:
   ```sh
   make NS
   ```
2. **Compile and Run the Storage Server:**:
    ```sh
    make SS
    ```
3. **Compile and Run the Client:**:
    ```sh
    make client
    ```

## File Structure

### Naming Server
**files/**
- ***cl.h***: Handles function operations.
- ***lru.h***: Implements the Least Recently Used (LRU) cache mechanism.
- ***misc.h***: Contains hashtables for efficient searching.
- ***ss.h***: Manages Storage Server handlers.

***ns.c***: The main implementation file for the Naming Server.

### Storage Server
**SS.c**: The main implementation file for the Storage Server, handling file operations and communication with the Naming Server and clients.

### Client
**client.c**: The main implementation file for the Client, handling user commands and interactions with the Naming Server and Storage Servers.


