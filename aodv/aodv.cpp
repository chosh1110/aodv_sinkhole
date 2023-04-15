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
#include <set>
#include <map>
using namespace std;

const int NETWORK_SIZE = 30;
const int NODE_COUNT = 20;
const double TRANSMISSION_RANGE = 10.0;

mutex mtx;

struct RREQ {
    int hop = 1;
    int des_ip;
    int des_seq = 0;
    int org_ip;
    int org_seq = 0;
    int prev_hop;
};

struct RREP {
    int hop=0;
    int dest_ip;
    int dest_seq=0;
    int org_ip;
    int org_seq;
    int prev_hop;
};

struct RERR {
    int un_dest_ip;
    int un_dest_seq;
};
struct RoutingTable {
    int nexthop;
    int hop;
    int dest_seq;
};
struct RREQ_PREV_HOP_NODE {
    int des_ip;
    int org_ip;
    int org_seq;
};
class Node {
    int node_x;
    int node_y;
    int id;
    int prev_org_seq = 0;
    int rrep_count = 0;
    int cnt = 0;
    vector<int> neighbor;
    queue<RREQ> rreq_buffer;
    queue<RREP> rrep_buffer;
    vector<RREQ> received_rreqs;
    vector <RREP> received_rreps;
    map <int,RoutingTable> routing_table;             
    map <tuple<int, int, int>,int> previous_hop_rreq;
    map <tuple<int, int, int>, int> previous_hop_rrep;
    map <tuple<int, int, int>, int> sent_rreq;
    condition_variable cv;
    mutex cv_m;
    map<int, int> rreq_prev_node;
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
    queue<RREQ> getRREQBuffer() {
        return rreq_buffer;
    }
    queue<RREP> getRREPBuffer() {
        return rrep_buffer;
    }
    vector<int> getNeighbor() {
        return neighbor;
    }
    map <int, RoutingTable> getRoutingTable() {
        return routing_table;
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
        unique_lock<mutex> lck(mtx);
        this->neighbor.clear();
        for (auto node : nodes) {
            if (node != this) {
                double distance = sqrt(pow(node->node_x - this->node_x, 2) + pow(node->node_y - this->node_y, 2));
                if (distance <= TRANSMISSION_RANGE) {
                    this->neighbor.push_back(node->id);
                }
            }
        }
        for (auto itr : routing_table) {
            int key = itr.first;
            RoutingTable value = itr.second;

            int nexthop = value.nexthop;
            bool exist = false;
            for (auto neighbor_node : neighbor) {
                if (nexthop == neighbor_node) {
                    exist = true;
                }
            }
            if (exist == false) {
                cout << "#CAUTION# ROUTE ERROR in NODE " << this->getId() <<" DESTINATION NODE "<<key << " NEXT HOP NODE " << nexthop << endl;
                this->routing_table.erase(key);
                break;
            }
        }
    }
    void generate_rreq(const vector<Node*>& nodes) {
        srand(time(NULL));
        if (this->getId()!=1) {
            return;
        }
        /*if (this->getId() != 1) {
            return;
        }*/
        RREQ rreq;
        int destination = 8;
        while (destination == this->getId()) {
            srand(time(NULL));
            destination = rand() % NODE_COUNT;
        }
        auto route = routing_table.find(destination);
        if (route != routing_table.end()) {
            cout << "Route Exist | DESTINATION NODE -> Node " << destination << " NEXT HOP -> Node " << route->second.nexthop<<endl;
            return;
        }
        rreq.des_ip = destination;
        rreq.des_seq++;
        rreq.org_ip = this->getId();
        this->prev_org_seq += 1;
        rreq.org_seq = this->prev_org_seq;
        rreq.prev_hop = this->getId();
        /*if (rreq.org_seq > 1) {
            return;
        }*/
        cout << "Node " << this->getId() << " generated RREQ DESTINATION Node -> "<<rreq.des_ip <<" SEQUENCE NUM -> "<<rreq.org_seq << endl;

        broadcast_rreq(rreq, nodes);
    }

    void broadcast_rreq(RREQ rreq, const vector<Node*>& nodes) {
        // Lock mutex to prevent multiple threads from accessing the same node's buffer simultaneously
        //unique_lock<mutex> lck(mtx);

        for (auto node : nodes) {
            // Check if node is within transmission range and not the same node as the sender
            if (node != this && sqrt(pow(node->node_x - this->node_x, 2) + pow(node->node_y - this->node_y, 2)) <= TRANSMISSION_RANGE) {
                // Add RREQ to the node's buffer
                /*if (node->getId() == rreq.org_ip) {
                    if (this->sent_rreq[make_tuple(rreq.des_ip, rreq.org_ip, rreq.org_seq)] == 1) {
                        return;
                    }
                    this->sent_rreq[make_tuple(rreq.des_ip, rreq.org_ip, rreq.org_seq)] = 1;
                }*/
                rreq.prev_hop = this->getId();
                node->rreq_buffer.push(rreq);
                
                // Print RREQ information
                //cout << "Node " << this->getId() << " sent RREQ to Node " << node->getId() <<" RREQ SEQ_NUM: "<<rreq.org_seq << endl;
                //cout << "RREQ | Hop: " << rreq.hop << " | Destination IP: " << rreq.des_ip << " | Destination Sequence Number: " << rreq.des_seq << " | Originator IP: " << rreq.org_ip << " | Originator Sequence Number: " << rreq.org_seq << " | Previous Node: " << rreq.prev_hop << endl;
            }
        }
    }
    bool isFind(RREQ curr, vector<RREQ> prev) {
        bool same = false;
        for (const auto& a : prev) {
            if (curr.org_ip == a.org_ip and curr.des_ip == a.des_ip and curr.org_seq <= a.org_seq) {
                same = true;
                return same;
            }
        }
        return same;
    }
    void check_buffer(vector<Node*>& nodes) {
        // Lock mutex to prevent multiple threads from accessing the same node's buffer simultaneously
        if (rreq_buffer.empty() and rrep_buffer.empty()) {
            return;
        }

        while (!rreq_buffer.empty()) {

            RREQ rreq = rreq_buffer.front();
            rreq_buffer.pop();
            
            //cout << "Node " << this->getId() << "is receiving RREQ from Node " << rreq.prev_hop <<" RREQ SEQ_NUM: " << rreq.org_seq<< endl;
            // Check if RREQ has already been received
            if (rreq.org_ip == this->getId()) {
                return;
            }
            if (!isFind(rreq, this->received_rreqs)) {
                received_rreqs.push_back(rreq);
                previous_hop_rreq[make_tuple(rreq.des_ip, rreq.org_ip, rreq.org_seq)] = rreq.prev_hop;//rreq를 보낸 노드를 저장
                rreq.hop++;
                
                // Check if node is the destination node
                if (rreq.des_ip == this->getId()) {
                    // TODO: send RREP back to originator
                    RREP rrep;
                    rrep.dest_ip = this->getId();
                    rrep_count++;
                    rrep.dest_seq += rrep_count;
                    rrep.hop = rreq.hop-1;
                    rrep.org_ip = rreq.org_ip;
                    rrep.org_seq = rreq.org_seq;
                    cout << "Congraulation!!!!!!!! Node " << this->getId() << " received RREQ from Node " << rreq.prev_hop << " and is the destination node Seq:" << rreq.org_seq << endl;
                    //system("PAUSE");
                    cout << "DESTINATION Node "<<this->getId()<<" RREQ Previous Hop -> " << previous_hop_rreq[make_tuple(rrep.dest_ip, rrep.org_ip, rrep.org_seq)] << endl;
                    for (auto node : nodes) {
                        if (node->getId() == this->previous_hop_rreq[make_tuple(rreq.des_ip, rreq.org_ip, rreq.org_seq)]) {
                            rrep.prev_hop = this->getId();
                            node->rrep_buffer.push(rrep);
                            cout << "Node " << this->getId() << " sent RREP to Node " << node->getId() << " DESTINATION NODE -> " << rrep.dest_ip << " SEQUENCE NUMBER -> " << rrep.dest_seq << endl;
                        }
                    }
                    //system("PAUSE");
                }
                else {
                    // Broadcast RREQ to neighbors
                    //cout << "Node " << this->getId() << " Previous Hop -> " << previous_hop_rreq[make_tuple(rreq.des_ip, rreq.org_ip, rreq.org_seq)] << endl;
                    broadcast_rreq(rreq, nodes);
                }
            }
            else {
                // RREQ has already been received, ignore
                //cout << "Node " << this->getId() << " received RREQ from Node "<< previous_hop_rreq[make_tuple(rreq.des_ip, rreq.org_ip, rreq.des_seq)]<<" but has already received it before from Node "<<received_rreqs.front().org_ip << endl;
            }
        }
        if (rrep_buffer.empty()) {
            return;
        }
        while (!rrep_buffer.empty()) {
            RREP rrep = rrep_buffer.front();
            rrep_buffer.pop();
            previous_hop_rrep[make_tuple(rrep.dest_ip, rrep.org_ip, rrep.org_seq)] = rrep.prev_hop;//rrep를 보낸 노드를 저장
            cout << "Node " << this->getId() << "is receiving RREP from Node " << rrep.prev_hop << endl;
            cout << "RREP Previous Hop -> " << previous_hop_rrep[make_tuple(rrep.dest_ip, rrep.org_ip, rrep.org_seq)] << endl;

            if (rrep.org_ip == this->getId()) {
                cout <<"NODE "<<this->getId() << " Route Discovery Finished SOURCE NODE : "<<rrep.org_ip<<" DESTINATION NODE : "<<rrep.dest_ip << " SEQUENCE NUMBER : "<<rrep.org_seq<<endl;
                //cout << "Node " << this->getId() << " storing route information, Destination -> Node " << rrep.dest_ip << " Next Hop -> Node " << previous_hop_rrep[make_tuple(rrep.dest_ip, rrep.org_ip, rrep.org_seq)] << endl;
                RoutingTable route;
                route.dest_seq = rrep.dest_seq;
                route.hop = rrep.hop;
                route.nexthop = previous_hop_rrep[make_tuple(rrep.dest_ip, rrep.org_ip, rrep.org_seq)];
                routing_table[rrep.dest_ip] = route;
                //system("PAUSE");
                return;
            }
            if (rrep.org_ip != this->getId()) {
                rrep.hop--;
                cout << "Node " << this->getId() << "is " << rrep.hop << " hop away from source Node "<<rrep.org_ip << endl;
                for (auto node : nodes) {
                    if (node != this && sqrt(pow(node->node_x - this->node_x, 2) + pow(node->node_y - this->node_y, 2)) <= TRANSMISSION_RANGE) {
                        if (node->getId() == this->previous_hop_rreq[make_tuple(rrep.dest_ip, rrep.org_ip, rrep.org_seq)]) {
                            rrep.prev_hop = this->getId();
                            node->rrep_buffer.push(rrep);
                            //cout << "Node " << this->getId() << " storing route information, Destination -> Node " << rrep.dest_ip << " Next Hop -> Node " << previous_hop_rrep[make_tuple(rrep.dest_ip, rrep.org_ip, rrep.org_seq)] << endl;
                            RoutingTable route;
                            route.dest_seq = rrep.dest_seq;
                            route.hop = rrep.hop;
                            route.nexthop = previous_hop_rrep[make_tuple(rrep.dest_ip, rrep.org_ip, rrep.org_seq)];
                            routing_table[rrep.dest_ip] = route;
                            //cout << "Complete storing route information Node " << this->getId() << " Destination -> Node " << rrep.dest_ip << " Next Hop -> Node " << routing_table[rrep.dest_ip].nexthop <<"Destination Sequence -> "<<routing_table[rrep.dest_ip].dest_seq << endl;
                            cout << "Node " << this->getId() << " sent RREP to Node " << node->getId() << " DESTINATION NODE -> "<<rrep.dest_ip<<" SEQUENCE NUMBER -> "<<rrep.dest_seq << endl;
                        }
                    }
                    
                }
            }
            
        }
    }
};
void print_node_log(const vector<Node*>& nodes, int node_id) {
    cout << "Log for Node " << node_id << ":" << endl;
    for (auto node : nodes) {
        if (node->getId() == node_id) {
            queue<RREQ> buffer_copy = node->getRREQBuffer();
            while (!buffer_copy.empty()) {
                RREQ rreq = buffer_copy.front();
                cout << "RREQ | Hop: " << rreq.hop << " | Destination IP: " << rreq.des_ip << " | Destination Sequence Number: " << rreq.des_seq << " | Originator IP: " << rreq.org_ip << " | Originator Sequence Number: " << rreq.org_seq << endl;
                buffer_copy.pop();
            }
        }
    }
}

void printNetwork(const vector<Node*>& nodes) {
    const int grid_size = 30;
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
    int cnt = 0;
    for (int i = 0; i < NODE_COUNT; i++) {
        nodes.push_back(new Node(i));
    }
    while (1) {
        vector<thread> threads;

        for (auto node : nodes) {
            threads.push_back(thread(&Node::move, node, ref(nodes)));
            threads.push_back(thread(&Node::hello_packet, node, ref(nodes)));
            for (auto& thread : threads) {
                thread.join();
            }
            threads.clear();
            threads.push_back(thread(&Node::generate_rreq, node, ref(nodes)));

            for (auto node : nodes) {
                threads.push_back(thread(&Node::check_buffer, node, ref(nodes)));
            }
            
            for (auto& thread : threads) {
                thread.join();
            }

            threads.clear();

            /*threads.push_back(thread(&Node::generate_rreq, node, ref(nodes)));
            for (auto& thread : threads) {
                thread.join();
            }
            threads.clear();*/
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
        for (auto node : nodes) {
            for (auto route : node->getRoutingTable()) {
                cout << "Node " << node->getId() << " routing table: Destination -> " << route.first << " Next Hop -> " << route.second.nexthop << endl;
            }
            
        }
        /*for (auto node : nodes) {
            print_node_log(nodes, node->getId());
        }*/
        // Print the network grid
        printNetwork(nodes);
         cout << endl;
    }
    return 0;
}
