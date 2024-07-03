#include <iostream>
#include <vector>
#include <stack>

using namespace std;

void dfs(int v, vector<bool> &visited, stack<int> &Stack, vector<vector<int>> &adj)
{
    visited[v] = true;
    for (size_t i = 0; i < adj[v].size(); ++i)
    {
        if (adj[v][i] && !visited[i])
        {
            dfs(i, visited, Stack, adj);
        }
    }
    Stack.push(v);
}

void reverseDfs(int v, vector<bool> &visited, vector<vector<int>> &revAdj, vector<int> &component)
{
    visited[v] = true;
    component.push_back(v);
    for (size_t i = 0; i < revAdj[v].size(); ++i)
    {
        if (revAdj[v][i] && !visited[i])
        {
            reverseDfs(i, visited, revAdj, component);
        }
    }
}

void kosaraju(int vertices, vector<vector<int>> &adj)
{
    vector<vector<int>> revAdj(vertices, vector<int>(vertices, 0));
    for (int i = 0; i < vertices; ++i)
    {
        for (int j = 0; j < vertices; ++j)
        {
            if (adj[i][j])
            {
                revAdj[j][i] = 1;
            }
        }
    }

    stack<int> Stack;
    vector<bool> visited(vertices, false);
    for (int i = 0; i < vertices; i++)
    {
        if (!visited[i])
        {
            dfs(i, visited, Stack, adj);
        }
    }

    fill(visited.begin(), visited.end(), false);

    while (!Stack.empty())
    {
        int v = Stack.top();
        Stack.pop();

        if (!visited[v])
        {
            vector<int> component;
            reverseDfs(v, visited, revAdj, component);
            cout << "SCC:";
            for (int vertex : component)
                cout << " " << (vertex + 1);
            cout << endl;
        }
    }
}

int main()
{
    int vertices, edges;
    cin >> vertices >> edges;
    vector<vector<int>> adj(vertices, vector<int>(vertices, 0));
    for (int i = 0; i < edges; i++)
    {
        int u, v;
        cin >> u >> v;
        adj[u - 1][v - 1] = 1;
    }
    kosaraju(vertices, adj);
    return 0;
}
