#ifndef SERVER_USING_PROACTTOR
#define SERVER_USING_PROACTTOR

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <sys/mman.h>
#include "libraries.cpp"
#include "Graph.cpp"

using namespace std;

#define PORT "9034" // Port we're listening on

pthread_mutex_t mutex;                   // Mutex for synchronizing access to clients
pthread_mutex_t graph_mutex;             // Mutex for synchronizing access to the graph
pthread_cond_t scc_cond;                 // Condition variable for SCC thread
int command_stdin_fd, command_stdout_fd; // Command's stdin and stdout file descriptors
vector<int> clients;                     // List of client sockets

bool scc_condition_met = false;      // Condition flag for SCC check
bool prev_scc_condition = false;     // Previous SCC condition
Graph *graph = Graph::getInstance(); // Single instance of the graph

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Return a listening socket
int get_listener_socket(void)
{
    int listener; // Listening socket descriptor
    int yes = 1;  // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
    {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0)
        {
            continue;
        }

        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai); // All done with this

    // If we got here, it means we didn't get bound
    if (p == NULL)
    {
        return -1;
    }

    // Listen
    if (listen(listener, 10) == -1)
    {
        return -1;
    }

    return listener;
}

// Function to run a command and get its stdin and stdout file descriptors
void run_command_and_get_pipes(const char *command, int *stdin_fd, int *stdout_fd)
{
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1)
    {
        perror("pipe");
        exit(1);
    }
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
    else if (pid == 0)
    {
        // Child process
        close(stdin_pipe[1]);              // Close the write end of the stdin pipe
        dup2(stdin_pipe[0], STDIN_FILENO); // Redirect stdin to the read end of the pipe
        close(stdin_pipe[0]);

        close(stdout_pipe[0]);               // Close the read end of the stdout pipe
        dup2(stdout_pipe[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe
        close(stdout_pipe[1]);

        execlp("bash", "bash", "-c", command, (char *)NULL);
        perror("execlp");
        exit(1);
    }
    else // Parent process
    {
        close(stdin_pipe[0]);  // Close the read end of the stdin pipe
        close(stdout_pipe[1]); // Close the write end of the stdout pipe

        *stdin_fd = stdin_pipe[1];   // Write end of the stdin pipe
        *stdout_fd = stdout_pipe[0]; // Read end of the stdout pipe
    }
}

// Function to handle client connections
void *handle_client(void *client_socket)
{
    int client_fd = *((int *)client_socket);
    char buf[256];
    int nbytes;

    // Send welcome message to the client
    const char *welcome_msg = "Which action do you want to perform?\n";
    if (send(client_fd, welcome_msg, strlen(welcome_msg), 0) == -1)
    {
        perror("send");
        close(client_fd);
        return NULL;
    }

    while ((nbytes = recv(client_fd, buf, sizeof buf, 0)) > 0)
    {
        pthread_mutex_lock(&mutex);
        // Write client data to the command's stdin
        write(command_stdin_fd, buf, nbytes);
        pthread_mutex_unlock(&mutex);

        // Check for SCC condition based on client input
        if (string(buf, nbytes).find("K") != string::npos)
        {
            pthread_mutex_lock(&graph_mutex);
            scc_condition_met = true;
            pthread_cond_signal(&scc_cond);
            pthread_mutex_unlock(&graph_mutex);
        }
    }

    if (nbytes == 0)
    {
        printf("server: socket %d hung up\n", client_fd);
    }
    else
    {
        perror("recv");
    }

    pthread_mutex_lock(&mutex);
    clients.erase(remove(clients.begin(), clients.end(), client_fd), clients.end());
    pthread_mutex_unlock(&mutex);

    close(client_fd);
    return NULL;
}

// Function to read from the command's stdout and send to clients
void *read_command_output(void *arg)
{
    char buf[256];
    int nbytes;

    while ((nbytes = read(command_stdout_fd, buf, sizeof buf)) > 0)
    {
        pthread_mutex_lock(&mutex);
        // Send the command's stdout to all clients
        for (int client_fd : clients)
        {
            if (send(client_fd, buf, nbytes, 0) == -1)
            {
                perror("send");
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    if (nbytes == 0)
    {
        printf("server: command stdout closed\n");
    }
    else
    {
        perror("read");
    }

    close(command_stdout_fd);
    return NULL;
}

// Function to check SCC condition and notify
void *check_scc_condition(void *arg)
{
    while (true)
    {
        pthread_mutex_lock(&graph_mutex);
        while (!scc_condition_met)
        {
            pthread_cond_wait(&scc_cond, &graph_mutex);
        }
        // Check if at least 50% of the graph belongs to the same SCC
        int largest_scc_size = graph->get_max_scc();
        int vertex_count = graph->getVertexCount();
        bool current_scc_condition = (largest_scc_size >= (vertex_count / 2) + 1);

        if (current_scc_condition)
        {
            cout << "At Least 50% of the graph belongs to the same SCC\n";
        }
        else if (prev_scc_condition)
        {
            cout << "At Least 50% of the graph no longer belongs to the same SCC\n";
        }

        prev_scc_condition = current_scc_condition;
        scc_condition_met = false; // Reset the condition flag after processing
        pthread_mutex_unlock(&graph_mutex);
    }
    return NULL;
}

// Main server function to handle incoming connections
void *server_function(void *arg)
{
    struct server_data
    {
        int listener;
        Proactor *proactor;
    };

    server_data *data = static_cast<server_data *>(arg);
    int listener = data->listener;
    Proactor *proactor = data->proactor;

    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;
    int newfd;
    char remoteIP[INET6_ADDRSTRLEN];

    while (true)
    {
        addrlen = sizeof remoteaddr;
        newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

        if (newfd == -1)
        {
            perror("accept");
            continue;
        }

        printf("server: new connection from %s on socket %d\n",
               inet_ntop(remoteaddr.ss_family,
                         get_in_addr((struct sockaddr *)&remoteaddr),
                         remoteIP, INET6_ADDRSTRLEN),
               newfd);

        pthread_mutex_lock(&mutex);
        clients.push_back(newfd);
        pthread_mutex_unlock(&mutex);

        int client_fd = newfd; // Use stack allocation for client_fd
        proactor->startProactor(client_fd, handle_client);
    }

    return NULL;
}

int main(void)
{
    // Initialize shared memory for the graph
    int shm_fd = shm_open("/graph_shared_memory", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        exit(1);
    }

    ftruncate(shm_fd, sizeof(Graph));
    graph = static_cast<Graph *>(mmap(NULL, sizeof(Graph), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
    if (graph == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&graph_mutex, NULL);
    pthread_cond_init(&scc_cond, NULL);

    int listener = get_listener_socket();

    if (listener == -1)
    {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    Proactor proactor;
    struct server_data
    {
        int listener;
        Proactor *proactor;
    } data;
    data.listener = listener;
    data.proactor = &proactor;

    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_function, &data);

    // Run the command and get its stdin and stdout file descriptors
    run_command_and_get_pipes("./list", &command_stdin_fd, &command_stdout_fd);

    pthread_t command_thread;
    pthread_create(&command_thread, NULL, read_command_output, NULL);

    pthread_t scc_thread;
    pthread_create(&scc_thread, NULL, check_scc_condition, NULL);

    pthread_join(server_thread, NULL);
    pthread_join(command_thread, NULL);
    pthread_join(scc_thread, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&graph_mutex);
    pthread_cond_destroy(&scc_cond);

    // Close shared memory
    munmap(graph, sizeof(Graph));
    close(shm_fd);
    shm_unlink("/graph_shared_memory");

    return 0;
}

#endif
