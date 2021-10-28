//
// Created by CGCL on 2021/9/28.
//

#ifndef ZSIM_BDFS_H
#define ZSIM_BDFS_H

#include <cstring>
#include <vector>
#include <map>
#include <stack>
#include <queue>
#include <thread>
#include <mutex>
#include <iostream>
#include "Edge.h"
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAX_DEPTH 10

using namespace std;

static pthread_mutex_t bcmtx, bfmtx, amtx;

template <typename T>
class BDFS {
private:
    vector<int> offset;
    vector<int> neighbor;
    vector<bool> *active_bits;
    vector<T> vertex_data;
    bool isPush;

    int current_vid;
    int last_vid;
    int cur_depth;

    stack<int> dfs_stack;
    queue<Edge> FIFO;
    mutex fifo_mutex;

    thread threads[THREAD_NUM];

private:
    // 将要以某个节点为u时，将其设置为unactive
    int scan()
    {
        for (int i = current_vid; i < last_vid; i++) {
            if ((*active_bits)[i]) {
                (*active_bits)[i] = false;
                current_vid++;
                dfs_stack.push(i);
                return i;
            }
        }
        return -1;
    }

    void fetch_neighbors()
    {
        int vid, start_offset, end_offset, last_level = -1;
        bool depin;
        cur_depth = 0;
        while (!dfs_stack.empty())
        {
            vid = dfs_stack.top();
            start_offset = offset[vid];
            end_offset = offset[vid + 1];
            depin = false;
            for (int i = start_offset; i < end_offset; ++i) {
                while (this->FIFO.size() > MAX_DEPTH)
                    this_thread::yield();               //fifo满时HATS停止
                lock_guard<mutex> lock(fifo_mutex);
                FIFO.push(Edge(vid, neighbor[i]));
                pthread_mutex_lock(&amtx);
                if (cur_depth < MAX_DEPTH && (*active_bits)[neighbor[i]]) {
                    (*active_bits)[neighbor[i]] = false;
                    dfs_stack.push(neighbor[i]);
                    depin = true;
                }
                pthread_mutex_unlock(&amtx);
            }
            if (depin)
                cur_depth++;
            else
            {
                dfs_stack.pop();
                cur_depth--;
            }
        }
    }

    T prefetch(int vid)
    {
        return vertex_data[vid];
    }

    void bdfs()
    {
        int top_id;
        while (dfs_stack.size() <= MAX_DEPTH) {
            top_id = dfs_stack.top();

        }
    }

public:
    void start()
    {
        int vid, start_offset, end_offset;
        vector <Edge> edges;
        while (current_vid < last_vid) {
            vid = scan();
            edges = fetch_neighbors(vid);
        }
        current_vid++;
        cout << "traverse end" << endl;
    }

    VO(){};
    ~VO()= default;

    void configure(vector<int> _offset, vector<int> _neighbor, vector<bool> *_active, vector<T> _vertex_data, bool _isPush,
                   int _start_v, int _end_v)
    {
        offset = _offset;
        neighbor = _neighbor;
        active_bits = _active;
        vertex_data = _vertex_data;
        isPush = _isPush;
        current_vid = _start_v;
        last_vid = _end_v;
//        cout << offset.size() << " " << neighbor.size() << " " << current_vid << " " << last_vid << endl;
    }

    void fetchEdges(Edge &edge)
    {
        while (this->FIFO.empty() && current_vid <= last_vid)
            this_thread::yield();           //fifo为空时fetch停止
        lock_guard<mutex> lock(fifo_mutex);
        if (!FIFO.empty())
        {
            edge = FIFO.front();
            FIFO.pop();
        }
        else
            edge = Edge(-1, -1);
    }
};

inline void hats_bdfs_configure(vector<int> *_offset, vector<int> *_neighbor, vector<bool> *_active, bool _isPush, int _start_v, int _end_v)
{
    int temp = (int) _isPush;
    vector<int> vertex_data(10, 5), *p = &vertex_data;

    int shmId = shmget((key_t)1234, 100, 0666|IPC_CREAT); //获取共享内存标志符
    void *addr = shmat(shmId, NULL, 0); //获取共享内存地址
    if (!addr)
    {
        printf("failed!!!\n");
        return;
    }

    pthread_mutex_lock(&bcmtx);
    memcpy((char *)addr, (void*)&_offset, 8);
    memcpy((char *)addr + 8, &_neighbor, 8);
    memcpy((char *)addr + 16, &_active, 8);
    memcpy((char *)addr + 24, &temp, 4);
    memcpy((char *)addr + 28, &_start_v, 4);
    memcpy((char *)addr + 32, &_end_v, 4);
    memcpy((char *)addr + 36, &p, 8);
    shmdt(addr);
    __asm__ __volatile__("xchg %r15, %r15");
    pthread_mutex_unlock(&bcmtx);
}

inline Edge hats_bdfs_fetch_edge()
{
    pthread_mutex_lock(&bfmtx);
    __asm__ __volatile__("xchg %rdx, %rdx");
    Edge edge;
    int shmId = shmget((key_t)1234, 100, 0666|IPC_CREAT); //获取共享内存标志符
    void *address = shmat(shmId, NULL, 0); //获取共享内存地址

    memcpy(&edge.u,(char *)address+80,4);
    memcpy(&edge.v,(char *)address+84,4);
    shmdt(address);
    pthread_mutex_unlock(&bfmtx);
    return edge;
}

#endif //ZSIM_BDFS_H
