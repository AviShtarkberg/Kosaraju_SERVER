# Graph Algorithms and Server Implementations

## Project Overview

This project focuses on implementing the kasaraju algorithm and use server designs utilizing different patterns such as Reactor and Proactor. The primary goals of this project include:

1. **Graph Implementation**: Developing a robust graph class that supports operations like adding edges, removing edges, and identifying strongly connected components (SCCs) using Kosaraju's algorithm.
2. **Server Implementations**:
   - **Beej's Chats server**: Server_chat.cpp uses the beej chat to allow clients to connect to the server.
   - **Threads server**: Server_threads.cpp uses threads to allow moultiple clients to connect.  
   - **Reactor Pattern**: Efficiently handles multiple client connections using the `select` system call.
   - **Proactor Pattern**: Manages asynchronous I/O operations and utilizes threads for handling client interactions effectively.

## Project Structure

- **Graph Implementation**: Found in `Graph.cpp`.
- **Server Implementations**:
  - `server_chat.cpp`: Using the beej chat from "beej's guide for networking".
  - `server_threads.cpp`: A server that manages client connections using threads.
  - `server_using_reactor.cpp`: Implements the Reactor pattern.
  - `server_using_proactor.cpp`: Implements the Proactor pattern.
- **Graph Operations via List, Deque, and Adjacency Matrix**:
  - `p1_using_list.cpp`
  - `p1_using_deque.cpp`
  - `p1_using_adj_matrix.cpp`
- **Library for proactor and Reactors**: Implemented in `libraries.cpp`.
- **Build Management**: Controlled through a `Makefile`.

### Build Instructions

1. **Compile the project**:
   - Use the following command to compile all targets:
     ```bash
     make all
     ```

**execution**:

1. list:
```bash
./list
```

2. Chat Server:
Executable: chat
Usage:
```bash
./chat
```

3. Threaded Server:
Executable: threads
Usage:
bash
```bash
./threads
```

4. Reactor-based Server:
Executable: reactor
Usage:
```bash
./reactor
```

5. Proactor-based Server:
Executable: proactor
Usage:
```bash
./proactor
```
### Usage:
- If you run ./list note that all the io will be in from and to stdin and stdout.(run just here and only ./list).
- Run the server with the implemention that you wish.
- Open a new terminal or multiple new terminals.
- In the terminal write : telnet 127.0.0.1 9034 to connect to the server that is running.
- Than ask for a Newgraph opertion in one of the clients like this:
    Newgraph 5,5 to create a graph with 5 vertices and 4 edges.
- Than you can choose which action to preform:
    Newedge 1,2 to add an edge from vertex 1 to vertex 2.
    Removeedge 1,2 to remove the edge from vertex 1 to vertex 2.
    K to find and print all SCCs in the graph.
- note that if you run the proactor file and you have more than 50% of the graph in the same connected component than you will get a notifiction about that in the server stdout.

### Profiling:
- At the gcov folder you can find all the profiling test that was done to determine which of the graph implemention was better to use in this project. The input.txt represent a complected graph that test the implamantions.