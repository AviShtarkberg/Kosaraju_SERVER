#include <iostream>
#include <sys/select.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include "libraries.cpp"

// Class for handling command output and sending it to clients
class CommandHandler : public EventHandler
{
private:
    int stdout_fd;               // File descriptor for the command's stdout
    std::map<int, bool> clients; // Map of client file descriptors to their active status
    Reactor *reactor;            // Pointer to the reactor

public:
    CommandHandler(int fd, Reactor *reactor) : stdout_fd(fd), reactor(reactor) {}

    // Add a new client to the handler
    void add_client(int client_fd)
    {
        clients[client_fd] = true;
    }

    // Remove a client from the handler
    void remove_client(int client_fd)
    {
        clients.erase(client_fd); // Ensure client is removed from the map
    }

    // Handle events from the command's stdout
    void handle_event() override
    {
        char buf[256];
        int nbytes = read(stdout_fd, buf, sizeof buf);
        if (nbytes <= 0)
        {
            if (nbytes == 0)
            {
                std::cout << "pollserver: command stdout closed\n";
            }
            else
            {
                perror("read");
            }
            close(stdout_fd);
            reactor->removeFdFromReactor(stdout_fd);
            clients[stdout_fd] = false;
        }
        else
        {
            for (std::map<int, bool>::iterator it = clients.begin(); it != clients.end(); ++it)
            {
                if (it->second == true)
                {
                    if (send(it->first, buf, nbytes, 0) == -1)
                    {
                        perror("send");
                        close(it->first); // Close and remove the bad FD
                        reactor->removeFdFromReactor(it->first);
                        it->second = false; // Mark client as invalid
                    }
                }
            }
        }
    }
};

// Class for handling client input and sending it to the command's stdin
class ClientHandler : public EventHandler
{
private:
    int client_fd;               // File descriptor for the client
    int command_stdin_fd;        // File descriptor for the command's stdin
    Reactor *reactor;            // Pointer to the reactor
    CommandHandler *cmd_handler; // Pointer to the command handler

public:
    ClientHandler(int fd, int cmd_stdin_fd, Reactor *reactor, CommandHandler *cmd_handler)
        : client_fd(fd), command_stdin_fd(cmd_stdin_fd), reactor(reactor), cmd_handler(cmd_handler) {}

    // Handle events from the client
    void handle_event() override
    {
        char buf[256];
        int nbytes = recv(client_fd, buf, sizeof buf, 0);
        if (nbytes <= 0)
        {
            if (nbytes == 0)
            {
                std::cout << "pollserver: socket " << client_fd << " hung up\n";
            }
            else
            {
                perror("recv");
            }
            close(client_fd);
            reactor->removeFdFromReactor(client_fd);
            cmd_handler->remove_client(client_fd); // Ensure client is removed from the CommandHandler
        }
        else
        {
            write(command_stdin_fd, buf, nbytes);
        }
    }
};

// Class for handling new incoming client connections
class ListenerHandler : public EventHandler
{
private:
    int listener_fd;             // File descriptor for the listener socket
    Reactor *reactor;            // Pointer to the reactor
    CommandHandler *cmd_handler; // Pointer to the command handler
    int command_stdin_fd;        // File descriptor for the command's stdin

public:
    ListenerHandler(int fd, Reactor *reactor, CommandHandler *cmd_handler, int cmd_stdin_fd)
        : listener_fd(fd), reactor(reactor), cmd_handler(cmd_handler), command_stdin_fd(cmd_stdin_fd) {}

    // Handle events from the listener socket
    void handle_event() override
    {
        struct sockaddr_storage remoteaddr;
        socklen_t addrlen = sizeof remoteaddr;
        int newfd = accept(listener_fd, (struct sockaddr *)&remoteaddr, &addrlen);
        if (newfd == -1)
        {
            perror("accept");
            return;
        }
        const char *welcome_msg = "Which action do you want to perform?\n";
        if (send(newfd, welcome_msg, strlen(welcome_msg), 0) == -1)
        {
            perror("send");
            close(newfd);
            return;
        }

        cmd_handler->add_client(newfd);
        reactor->addFdToReactor(newfd, new ClientHandler(newfd, command_stdin_fd, reactor, cmd_handler), true);
    }
};

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
        close(stdin_pipe[1]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);

        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[1]);

        execlp("bash", "bash", "-c", command, (char *)NULL);
        perror("execlp");
        exit(1);
    }
    else
    {
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        *stdin_fd = stdin_pipe[1];
        *stdout_fd = stdout_pipe[0];
    }
}

// Function to get a listening socket
int get_listener_socket(void)
{
    int listener;
    int yes = 1;
    int rv;

    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, "9034", &hints, &ai)) != 0)
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

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai);

    if (p == NULL)
    {
        return -1;
    }

    if (listen(listener, 10) == -1)
    {
        return -1;
    }

    return listener;
}

int main(void)
{
    Reactor reactor;
    int listener = get_listener_socket();

    if (listener == -1)
    {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    int command_stdin_fd, command_stdout_fd;
    run_command_and_get_pipes("./list", &command_stdin_fd, &command_stdout_fd);

    CommandHandler *cmd_handler = new CommandHandler(command_stdout_fd, &reactor);
    reactor.addFdToReactor(command_stdout_fd, cmd_handler, true);

    ListenerHandler *listener_handler = new ListenerHandler(listener, &reactor, cmd_handler, command_stdin_fd);
    reactor.addFdToReactor(listener, listener_handler, true);

    reactor.startReactor();

    return 0;
}
