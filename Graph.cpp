#ifndef GRAPH_H
#define GRAPH_H

#include <iostream>
#include <vector>
#include <stack>
#include <list>
#include <limits>
#include <algorithm>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

class Graph
{
private:
    int vertices; // Number of vertices in the graph
    int max_css; // Maximum size of the Strongly Connected Components (SCCs)
    vector<pair<int, int>> edgeList; // List of edges in the graph
    vector<list<int>> adj; // Adjacency list for the graph
    vector<list<int>> revAdj; // Reverse adjacency list for the graph

    // Private constructor to prevent multiple instances
    Graph() : vertices(0) {}

    // Depth First Search (DFS) function used for Kosaraju's algorithm
    void dfs(int v, vector<bool> &visited, stack<int> &Stack)
    {
        visited[v] = true;
        for (auto i = adj[v].begin(); i != adj[v].end(); ++i)
        {
            if (!visited[*i])
            {
                dfs(*i, visited, Stack);
            }
        }
        Stack.push(v);
    }

    // Reverse DFS function used for Kosaraju's algorithm
    void reverseDfs(int v, vector<bool> &visited, vector<int> &component)
    {
        visited[v] = true;
        component.push_back(v);
        for (auto i = revAdj[v].begin(); i != revAdj[v].end(); ++i)
        {
            if (!visited[*i])
            {
                reverseDfs(*i, visited, component);
            }
        }
    }

public:
    // Singleton pattern to get the unique instance of Graph class
    static Graph *getInstance()
    {
        static Graph *instance = nullptr;
        if (instance == nullptr)
        {
            // Shared memory setup
            int shm_fd = shm_open("/graph_shared_memory", O_CREAT | O_RDWR, 0666);
            if (shm_fd == -1)
            {
                perror("shm_open");
                exit(1);
            }

            ftruncate(shm_fd, sizeof(Graph));

            // Map shared memory to instance
            instance = static_cast<Graph *>(mmap(NULL, sizeof(Graph), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
            if (instance == MAP_FAILED)
            {
                perror("mmap");
                exit(1);
            }

            new (instance) Graph(); // Placement new to call constructor
        }
        return instance;
    }

    // Delete copy constructor and assignment operator to prevent copies
    Graph(const Graph &) = delete;
    void operator=(const Graph &) = delete;

    // Function to create a new graph with given vertices and edges
    void newGraph(int v, int e)
    {
        vertices = v;
        edgeList.clear();
        adj.clear();
        adj.resize(vertices);
        revAdj.clear();
        revAdj.resize(vertices);

        cout << "Enter the edges (format: u v):" << endl;
        for (int i = 0; i < e; i++)
        {
            int u, v;
            cin >> u >> v;
            newEdge(u, v);
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clear the input buffer
        cout << "The graph was created successfully" << endl;
    }

    // Function to find and print all Strongly Connected Components (SCCs) using Kosaraju's algorithm
    void kosaraju()
    {
        stack<int> Stack;
        vector<bool> visited(vertices, false);

        // Fill vertices in stack according to their finishing times
        for (int i = 0; i < vertices; i++)
        {
            if (!visited[i])
            {
                dfs(i, visited, Stack);
            }
        }

        // Reset visited array for second pass
        fill(visited.begin(), visited.end(), false);
        int largest_scc_size = 0;
        
        // Process all vertices in order defined by Stack
        while (!Stack.empty())
        {
            int v = Stack.top();
            Stack.pop();

            if (!visited[v])
            {
                vector<int> component;
                reverseDfs(v, visited, component);
                largest_scc_size = max(largest_scc_size, static_cast<int>(component.size()));
                cout << "SCC:";
                for (int vertex : component)
                    cout << " " << (vertex + 1);
                cout << endl;
            }
        }
        this->max_css = largest_scc_size;
    }

    // Function to add a new edge to the graph
    void newEdge(int u, int v)
    {
        edgeList.emplace_back(u, v);
        adj[u - 1].push_back(v - 1);
        revAdj[v - 1].push_back(u - 1);
        cout << "The edge " << u << "," << v << " was added" << endl;
    }

    // Function to remove an edge from the graph
    void removeEdge(int u, int v)
    {
        auto it = find(edgeList.begin(), edgeList.end(), make_pair(u, v));
        if (it != edgeList.end())
        {
            edgeList.erase(it);
            adj[u - 1].remove(v - 1);
            revAdj[v - 1].remove(u - 1);
            cout << "The edge " << u << "," << v << " was removed" << endl;
        }
        else
        {
            cout << "Edge " << u << "," << v << " not found" << endl;
        }
    }

    // Getter for the number of vertices in the graph
    int getVertexCount()
    {
        return vertices;
    }

    // Getter for the maximum size of the SCCs
    int get_max_scc()
    {
        return this->max_css;
    }
};

#endif
