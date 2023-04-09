#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <time.h>
#include <windows.h>
#include <random>
#include <stdlib.h>
#include <queue>
#include <mutex>
#include <unordered_set>
#include <string>
using namespace std;

const int NETWORK_SIZE = 50;
const int NODE_COUNT = 5;
const double TRANSMISSION_RANGE = 15.0;

mutex mtx;

struct RREQ {
    int hop = 0;
    int des_ip;
    int des_seq;
    int org_ip;
    int org_seq;
};

struct RREP {
    int hop;
    int dest_ip;
    int dest_seq;
    int org_ip;
    int life_time;
};

struct RERR {
    int un_dest_ip;
    int un_dest_seq;
};

class Node {
    int node_x;
    int node_y;
    int id;
    vector<int> neighbor;
    queue<RREQ> buffer;
    vector<RREQ> received_rreqs;
    condition_variable cv;
    mutex cv_m;

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
    queue<RREQ> getBuffer() {
        return buffer;
    }
    vector<int> getNeighbor() {
        return neighbor;
    }

    void move(const vector<Node*>& nodes) {
        static default_random_engine generator;
        static uniform_int_distribution<int> distribution(-1, 1);

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
                double distance = sqrt(pow(node->node_x - this->node_x, 2) + pow(node->node_y - this->node_y, 2));
                if (distance <= TRANSMISSION_RANGE) {
                    this->neighbor.push_back(node->id);
                }
            }
        }
    }
    void generate_rreq(const vector<Node*>& nodes) {
        RREQ rreq;
        rreq.des_ip = 6;
        rreq.des_seq = 123;
        rreq.org_ip = this->getId();
        rreq.org_seq = 123;
        cout << "Node " << this->getId() << " generated RREQ" << endl;
        broadcast_rreq(rreq, nodes);
    }


    void broadcast_rreq(RREQ rreq, const vector<Node*>& nodes) {
        // Lock mutex to prevent multiple threads from accessing the same node's buffer simultaneously
        lock_guard<mutex> guard(mtx);

        for (auto neighbor_id : this->getNeighbor()) {
            for (auto node : nodes) {
                if (node->getId() == neighbor_id) {
                    if (!node->received_rreqs.empty()) {//이전에 받은 rreq table이 비어있지 않다면
                        for (auto prev_rreq : node->received_rreqs) {
                            if (prev_rreq.org_ip == rreq.org_ip) {//목적지가 같은지 비교
                                if (prev_rreq.org_seq >= rreq.org_seq) {//목적지가 같으면 sequence number로 비교
                                    break;
                                }
                                else if (prev_rreq.org_seq < rreq.org_seq) {
                                    node->buffer.push(rreq);
                                    node->received_rreqs.push_back(rreq);
                                    cout << "Node " << this->getId() << " broadcast RREQ to node " << node->getId() << endl;
                                }
                            }
                        }
                    }
                    else {
                        node->buffer.push(rreq);
                        node->received_rreqs.push_back(rreq);
                        cout << "Node " << this->getId() << " broadcast RREQ to node " << node->getId() << endl;
                    }
                }
            }

        }
    }



    void receive_rreq(vector<Node*>& nodes) {
        // Lock mutex to prevent multiple threads from accessing the same node's buffer simultaneously
        unique_lock<mutex> ulock(mtx);

        while (!this->buffer.empty()) {
            RREQ rreq = this->buffer.front();
            this->buffer.pop();

            // Check if this node is the destination node
            if (rreq.des_ip == this->getId()) {
                cout << "I am the destination node with IP: " << this->getId() << endl;
            }
            else {
                // Broadcast the RREQ to the node's neighbors
                broadcast_rreq(rreq, nodes);
            }
            cout << "Node " << this->getId() << " processed RREQ" << endl;
        }
    }




};
void print_node_log(const vector<Node*>& nodes, int node_id) {
    cout << "Log for Node " << node_id << ":" << endl;
    for (auto node : nodes) {
        if (node->getId() == node_id) {
            queue<RREQ> buffer_copy = node->getBuffer();
            while (!buffer_copy.empty()) {
                RREQ rreq = buffer_copy.front();
                cout << "RREQ | Hop: " << rreq.hop << " | Destination IP: " << rreq.des_ip << " | Destination Sequence Number: " << rreq.des_seq << " | Originator IP: " << rreq.org_ip << " | Originator Sequence Number: " << rreq.org_seq << endl;
                buffer_copy.pop();
            }
        }
    }
}

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
            threads.push_back(thread(&Node::move, node, ref(nodes)));
            threads.push_back(thread(&Node::hello_packet, node, ref(nodes)));
            threads.push_back(thread(&Node::generate_rreq, node, ref(nodes)));
            threads.push_back(thread(&Node::receive_rreq, node, ref(nodes)));
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // Print each node's neighbors and current position
        for (auto node : nodes) {
            printf("Node Id:%2d | X:%3d | Y:%3d | Neighbor:", node->getId(), node->getCurrent_x(), node->getCurrent_y());
            for (int i = 0; i < node->getNeighbor().size(); i++)
            {
                printf("%3d", node->getNeighbor().at(i));
            }
            printf("\n");
        }

        // Print the network grid
        printNetwork(nodes);
        for (auto node : nodes) {
            print_node_log(nodes, node->getId());
        }
        cout << endl;
        Sleep(500);
    }

    return 0;
}
