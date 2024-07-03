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

using namespace std;

#define PORT "9034" // Port we're listening on

pthread_mutex_t mutex;                   // Mutex for synchronizing access to shared resources
int command_stdin_fd, command_stdout_fd; // Command's stdin and stdout file descriptors
vector<int> clients;                     // List of connected client sockets

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

        execlp("bash", "bash", "-c", command, (char *)NULL); // Execute the command
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
    free(client_socket);
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

    // Continuously receive data from the client and write it to the command's stdin
    while ((nbytes = recv(client_fd, buf, sizeof buf, 0)) > 0)
    {
        pthread_mutex_lock(&mutex);
        write(command_stdin_fd, buf, nbytes);
        pthread_mutex_unlock(&mutex);
    }

    if (nbytes == 0)
    {
        printf("server: socket %d hung up\n", client_fd);
    }
    else
    {
        perror("recv");
    }

    // Remove client from the list of clients
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

    // Continuously read from the command's stdout and send it to all clients
    while ((nbytes = read(command_stdout_fd, buf, sizeof buf)) > 0)
    {
        pthread_mutex_lock(&mutex);
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

// Main server function to handle incoming connections
void *server_function(void *arg)
{
    int listener = *((int *)arg);
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;
    int newfd;
    char remoteIP[INET6_ADDRSTRLEN];

    // Continuously accept new connections and create threads to handle them
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

        pthread_t client_thread;
        int *client_socket = (int *)malloc(sizeof(int));
        *client_socket = newfd;
        pthread_create(&client_thread, NULL, handle_client, client_socket);
        pthread_detach(client_thread);
    }

    return NULL;
}

int main(void)
{
    pthread_mutex_init(&mutex, NULL);

    // Get the listener socket
    int listener = get_listener_socket();

    if (listener == -1)
    {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_function, &listener);

    // Run the command and get its stdin and stdout file descriptors
    run_command_and_get_pipes("./list", &command_stdin_fd, &command_stdout_fd);

    pthread_t command_thread;
    pthread_create(&command_thread, NULL, read_command_output, NULL);

    // Wait for server and command threads to finish
    pthread_join(server_thread, NULL);
    pthread_join(command_thread, NULL);

    pthread_mutex_destroy(&mutex);

    return 0;
}
