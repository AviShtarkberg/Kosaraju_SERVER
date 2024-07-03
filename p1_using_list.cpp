#include <iostream>
#include <vector>
#include <stack>
#include <string>
#include <sstream>
#include <algorithm>
#include <list>
#include <limits>
#include "Graph.cpp"
using namespace std;

int main()
{
    Graph *graph = Graph::getInstance(); // Get the singleton instance of the Graph
    while (true)
    {
        cout << "Enter the action that you want to perform:" << endl;
        string input;
        getline(cin, input); // Get user input
        istringstream iss(input);
        string action, params;
        iss >> action; // Extract the action command
        getline(iss, params); // Extract the parameters
        params.erase(remove(params.begin(), params.end(), ' '), params.end()); // Remove spaces from parameters

        if (action == "Newgraph")
        {
            size_t commaPos = params.find(',');
            if (commaPos != string::npos)
            {
                // Parse the number of vertices and edges
                int vertices = stoi(params.substr(0, commaPos));
                int edges = stoi(params.substr(commaPos + 1));
                graph->newGraph(vertices, edges); // Create a new graph
            }
            else
            {
                cout << "Invalid parameters for Newgraph. Please use the format 'Newgraph vertices,edges'." << endl;
            }
        }
        else if (action == "K")
        {
            // Perform Kosaraju's algorithm to find SCCs
            cout << "Kosaraju on the current graph: " << endl;
            graph->kosaraju();
        }
        else if (action == "Newedge")
        {
            size_t commaPos = params.find(',');
            if (commaPos != string::npos)
            {
                // Parse the vertices for the new edge
                int u = stoi(params.substr(0, commaPos));
                int v = stoi(params.substr(commaPos + 1));
                graph->newEdge(u, v); // Add a new edge to the graph
            }
            else
            {
                cout << "Invalid parameters for Newedge. Please use the format 'Newedge u,v'." << endl;
            }
        }
        else if (action == "Removeedge")
        {
            size_t commaPos = params.find(',');
            if (commaPos != string::npos)
            {
                // Parse the vertices for the edge to remove
                int u = stoi(params.substr(0, commaPos));
                int v = stoi(params.substr(commaPos + 1));
                graph->removeEdge(u, v); // Remove an edge from the graph
            }
            else
            {
                cout << "Invalid parameters for Removeedge. Please use the format 'Removeedge u,v'." << endl;
            }
        }
        else if (action == "end")
        {
            // Exit the program
            exit(0);
        }
        else
        {
            cout << "Invalid action. Available actions: Newgraph, K, Newedge, Removeedge, end." << endl;
        }
    }
    return 0;
}
