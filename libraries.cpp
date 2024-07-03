#ifndef LIB_H
#define LIB_H

#include <iostream>
#include <sys/select.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <pthread.h>

// Abstract base class for event handlers
class EventHandler
{
public:
    // Pure virtual function to handle an event
    virtual void handle_event() = 0;
};

// Reactor class for handling I/O events using the Reactor pattern
class Reactor
{
private:
    std::map<int, EventHandler *> handlers; // Map of file descriptors to their corresponding event handlers
    fd_set read_fds; // Set of file descriptors to monitor for reading
    fd_set write_fds; // Set of file descriptors to monitor for writing
    bool running; // Flag to control the reactor loop

public:
    // Constructor to initialize the Reactor
    Reactor()
    {
        FD_ZERO(&read_fds); // Initialize the read file descriptor set
        FD_ZERO(&write_fds); // Initialize the write file descriptor set
        running = false; // Initialize the running flag
    }

    // Add a file descriptor to the Reactor
    // fd: File descriptor to add
    // handler: Event handler associated with the file descriptor
    // is_read: True if monitoring for read events, false for write events
    int addFdToReactor(int fd, EventHandler *handler, bool is_read)
    {
        if (is_read)
        {
            FD_SET(fd, &read_fds); // Add to read file descriptor set
        }
        else
        {
            FD_SET(fd, &write_fds); // Add to write file descriptor set
        }
        handlers[fd] = handler; // Add handler to the map
        return 0;
    }

    // Remove a file descriptor from the Reactor
    // fd: File descriptor to remove
    int removeFdFromReactor(int fd)
    {
        FD_CLR(fd, &read_fds); // Remove from read file descriptor set
        FD_CLR(fd, &write_fds); // Remove from write file descriptor set
        handlers.erase(fd); // Remove handler from the map
        return 0;
    }

    // Start the Reactor to begin handling events
    void startReactor()
    {
        running = true; // Set running flag to true
        while (running)
        {
            fd_set temp_read_fds = read_fds; // Temporary read file descriptor set for select()
            fd_set temp_write_fds = write_fds; // Temporary write file descriptor set for select()

            // Monitor file descriptors for events
            int n = select(FD_SETSIZE, &temp_read_fds, &temp_write_fds, nullptr, nullptr);

            if (n < 0)
            {
                perror("select error"); // Handle select error
                break;
            }

            // Check which file descriptors have events
            for (int fd = 0; fd < FD_SETSIZE; ++fd)
            {
                if (FD_ISSET(fd, &temp_read_fds))
                {
                    handlers[fd]->handle_event(); // Handle read event
                }
                if (FD_ISSET(fd, &temp_write_fds))
                {
                    handlers[fd]->handle_event(); // Handle write event
                }
            }
        }
    }

    // Stop the Reactor from handling events
    void stopReactor()
    {
        running = false; // Set running flag to false
    }
};

// Proactor class for handling asynchronous I/O using the Proactor pattern
class Proactor
{
public:
    // Type alias for the function pointer used by the Proactor
    using proactorFunc = void *(*)(void *);

private:
    // Structure to represent a task handled by the Proactor
    struct ProactorTask
    {
        int sockfd; // Socket file descriptor
        proactorFunc func; // Function to execute for the task
        pthread_t thread_id; // Thread ID for the task
    };

    std::vector<ProactorTask> tasks; // Vector of tasks managed by the Proactor

public:
    // Start a new task in the Proactor
    // sockfd: Socket file descriptor for the task
    // threadFunc: Function to execute for the task
    // Returns the thread ID of the created task
    pthread_t startProactor(int sockfd, proactorFunc threadFunc)
    {
        ProactorTask task;
        task.sockfd = sockfd;
        task.func = threadFunc;
        if (pthread_create(&task.thread_id, nullptr, threadFunc, &task.sockfd) != 0)
        {
            perror("pthread_create"); // Handle thread creation error
            return -1;
        }
        tasks.push_back(task); // Add task to the vector
        return task.thread_id;
    }

    // Stop a running task in the Proactor
    // tid: Thread ID of the task to stop
    // Returns 0 on success, -1 on failure
    int stopProactor(pthread_t tid)
    {
        for (auto it = tasks.begin(); it != tasks.end(); ++it)
        {
            if (it->thread_id == tid)
            {
                if (pthread_cancel(tid) != 0)
                {
                    perror("pthread_cancel"); // Handle thread cancel error
                    return -1;
                }
                tasks.erase(it); // Remove task from the vector
                return 0;
            }
        }
        return -1; // Task not found
    }
};

#endif 
