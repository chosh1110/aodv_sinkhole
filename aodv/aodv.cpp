#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <time.h>
#include<windows.h>
#include <random>
#include<stdlib.h>
using namespace std;

const int NETWORK_SIZE = 50;
const int NODE_COUNT = 10;
const int TRANSMISSION_RANGE = 15;
struct RREQ {
    int hop;
    int des_ip;
    int des_seq;
    int org_ip;
    int org_seq;
};
class Node {
    int node_x;
    int node_y;
    int id;
    vector <int> neighbor;
public:
    Node(int node_id) {
        node_x = rand() % NETWORK_SIZE;
        node_y = rand() % NETWORK_SIZE;
        id = node_id;
    }

    int getCurrent_x() {
        return node_x;
    }

    int getCurrent_y() {
        return node_y;
    }

    int getId() {
        return id;
    }
    vector<int> getNeighbor() {
        return neighbor;
    }
    void move(const vector<Node*>& nodes) {
        static default_random_engine generator;
        static uniform_int_distribution<int> distribution(-3, 3);

        // Calculate new position
        int new_x = node_x + distribution(generator);
        int new_y = node_y + distribution(generator);

        // Check if new position overlaps with any other nodes
        bool overlaps = false;
        for (auto node : nodes) {
            if (node != this) {
                int distance = sqrt(pow(node->node_x - new_x, 2) + pow(node->node_y - new_y, 2));
                if (distance == 0) {
                    overlaps = true;
                    break;
                }
            }
        }

        // Update node position if it doesn't overlap with any other nodes
        if (!overlaps) {
            node_x = new_x;
            node_y = new_y;
        }

        // Ensure node stays within network bounds
        node_x = max(0, min(NETWORK_SIZE - 1, node_x));
        node_y = max(0, min(NETWORK_SIZE - 1, node_y));
    }

    void hello_packet(const vector<Node*>& nodes) {
        this->neighbor.clear();
        for (auto node : nodes) {
            if (node != this) {
                int distance = sqrt(pow(node->node_x - this->node_x, 2) + pow(node->node_y - this->node_y, 2));
                if (distance <= TRANSMISSION_RANGE) {
                    this->neighbor.push_back(node->id);
                }
            }
        }
    }
};
void printNetwork(const vector<Node*>& nodes) {
    const int grid_size = 50;
    char grid[grid_size][grid_size];

    // Initialize grid with dots
    for (int i = 0; i < grid_size; i++) {
        for (int j = 0; j < grid_size; j++) {
            grid[i][j] = '.';
        }
    }

    // Mark nodes on the grid
    for (auto node : nodes) {
        int x = node->getCurrent_x() * grid_size / NETWORK_SIZE;
        int y = node->getCurrent_y() * grid_size / NETWORK_SIZE;

        grid[x][y] = node->getId() + '0';
    }

    // Print the grid
    for (int i = 0; i < grid_size; i++) {
        for (int j = 0; j < grid_size; j++) {
            cout << grid[i][j] << " ";
        }
        cout << endl;
    }
}
int main() {
    vector<Node*> nodes;
    for (int i = 0; i < NODE_COUNT; i++) {
        nodes.push_back(new Node(i));
    }

    while (1) {
        vector<thread> threads;
        for (auto node : nodes) {
            threads.push_back(thread(&Node::hello_packet, node, ref(nodes)));
        }
        for (auto node : nodes) {
            printf("Node Id:%2d | X:%3d | Y:%3d | Neighbor:", node->getId(), node->getCurrent_x(), node->getCurrent_y());
            for (int i = 0; i < node->getNeighbor().size(); i++)
            {
                printf("%3d", node->getNeighbor().at(i));
            }
            printf("\n");
        }
        printNetwork(nodes);
        for (auto node : nodes) {
            threads.push_back(thread(&Node::move, node, ref(nodes)));
        }
        for (auto& thread : threads) {
            thread.join();
        }
        cout << endl;
        system("cls");
    }

    return 0;
}
