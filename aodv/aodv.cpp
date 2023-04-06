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
using namespace std;
const int NETWORK_SIZE = 50;
const int NODE_COUNT = 10;
const double TRANSMISSION_RANGE = 15.0;
mutex mtx;
struct RREQ {
    int hop = 0;
    int des_ip; //목적지 노드의 id 대체
    int des_seq;
    int org_ip; //출발지 노드의 id 대체
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
    vector <int> neighbor;
    queue <RREQ> buffer;
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
    vector<int> getNeighbor() {
        return neighbor;
    }
    void move(const vector<Node*>& nodes) {//노드 랜덤으로 움직이는 함수
        static default_random_engine generator;
        static uniform_int_distribution<int> distribution(-1,1);

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

    void hello_packet(const vector<Node*>& nodes) {//이웃노드 찾는 함수
        this->neighbor.clear();
        for (auto node : nodes) {
            if (node != this) {
                double distance = sqrt(pow(node->node_x - this->node_x, 2) + pow(node->node_y - this->node_y, 2));
                //printf("S: %2d D: %2d Distance:%f\n", this->getId(), node->getId(),distance);
                if (distance <= TRANSMISSION_RANGE) {
                    this->neighbor.push_back(node->id);                    
                }
            }
        }
    }
    void send_rreq(const vector<Node*>& nodes) {//RREQ Broadcast함수
        if (this->getNeighbor().empty()) {
            return;
        }
        RREQ rreq;
        static default_random_engine generator;
        static uniform_int_distribution<int> distribution(0, 100);
        int num = distribution(generator);
        if (num > 0) {
            static uniform_int_distribution<int> distribution(0, NODE_COUNT-1);
            rreq.hop += 1;
            for (int i = 0; i < NODE_COUNT; i++)
            {
                int dest_ip = distribution(generator);
                if (dest_ip == this->getId()) {
                    continue;
                }
                else {
                    rreq.des_ip = dest_ip;
                    break;
                }
            }
            rreq.des_seq = 15;
            rreq.org_ip = this->getId();
            rreq.org_seq = 23;

            for (auto neighbor : this->getNeighbor()) {
                for (auto node : nodes) {
                    if (node != this) {
                        double distance = sqrt(pow(node->node_x - this->node_x, 2) + pow(node->node_y - this->node_y, 2));
                        if (distance > TRANSMISSION_RANGE) {
                            continue;
                        }
                        else if (neighbor == node->getId()) {
                            printf("Node %2d sending RREQ to %2d\n", this->getId(), neighbor);
                            node->receive_rreq(rreq);
                        }
                    }
                }
            }
        }
    }
    void receive_rreq(RREQ rreq) {//RREQ 수신 함수
        if (rreq.des_ip == this->getId()) {
            //목적지 도착
        }
        else {
            this->buffer.push(rreq);
            //printf("Node %2d received RREQ from %2d\n", this->getId(),rreq.org_ip);
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
            threads.push_back(thread(&Node::move, node, ref(nodes)));
            threads.push_back(thread(&Node::hello_packet, node, ref(nodes)));
            threads.push_back(thread(&Node::send_rreq, node, ref(nodes)));
        }
        /*for (auto node : nodes) {
            threads.push_back(thread(&Node::hello_packet, node, ref(nodes)));
            //threads.push_back(thread(&Node::send_rreq, node, ref(nodes)));
        }*/

        for (auto& thread : threads) {
            thread.join();
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
        cout << endl;
        Sleep(500);
    }

    return 0;
}
