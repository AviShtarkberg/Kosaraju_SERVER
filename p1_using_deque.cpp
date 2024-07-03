#include <iostream>
#include <vector>
#include <stack>
#include <deque>

using namespace std;

void dfs(int v, vector<bool> &visited, stack<int> &Stack, vector<deque<int>> &adj)
{
    visited[v] = true;
    for (auto i = adj[v].begin(); i != adj[v].end(); ++i)
    {
        if (!visited[*i])
        {
            dfs(*i, visited, Stack, adj);
        }
    }
    Stack.push(v);
}

void reverseDfs(int v, vector<bool> &visited, vector<deque<int>> &revAdj, vector<int> &component)
{
    visited[v] = true;
    component.push_back(v);
    for (auto i = revAdj[v].begin(); i != revAdj[v].end(); ++i)
    {
        if (!visited[*i])
        {
            reverseDfs(*i, visited, revAdj, component);
        }
    }
}

void kosaraju(int vertices, vector<pair<int, int>> &edges)
{
    vector<deque<int>> adj(vertices);
    vector<deque<int>> revAdj(vertices);
    for (auto edge : edges)
    {
        adj[edge.first - 1].push_back(edge.second - 1);
        revAdj[edge.second - 1].push_back(edge.first - 1);
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
    vector<pair<int, int>> edgeList(edges);
    for (int i = 0; i < edges; i++)
    {
        cin >> edgeList[i].first >> edgeList[i].second;
    }
    kosaraju(vertices, edgeList);
    return 0;
}
