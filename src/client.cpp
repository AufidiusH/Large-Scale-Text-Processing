#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <sys/stat.h>
#include <climits>
#include <memory> // 新增：unique_ptr/make_unique 所需头文件

#include "gen-cpp/TextSearch.h"

// ===== 适配 Thrift 0.19 必须使用的头文件 =====
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransport.h>
#include <thrift/transport/TBufferTransports.h> // 替代 TBufferedTransport.h
#include <thrift/protocol/TBinaryProtocol.h>

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;

// 全局变量
vector<SearchResult> g_allResults;
mutex g_resultMutex;

// 重命名后的服务器节点结构体（含不可拷贝的mutex）
struct MyServerNode
{
    string ip;
    int port;
    int currentTasks;
    mutex nodeMutex;
    MyServerNode(string ip_, int port_) : ip(ip_), port(port_), currentTasks(0) {}
};

struct Task : public FileTask
{
    int taskSize;
    Task(FileTask ft, int size) : FileTask(ft), taskSize(size) {}
};

// 获取文件大小
long getFileSize(const string &fileName)
{
    struct stat statbuf;
    if (stat(fileName.c_str(), &statbuf) == -1)
        return 0;
    return statbuf.st_size;
}

// 文件任务分解
vector<Task> splitTasks(const vector<string> &fileNames)
{
    vector<Task> tasks;
    const int MAX_SMALL_TASK_SIZE = 1 * 1024;
    const int BIG_TASK_SLICE = 1 * 1024;

    vector<FileTask> smallFileTasks;
    long smallTotalSize = 0;

    for (const auto &fileName : fileNames)
    {
        long fileSize = getFileSize(fileName);
        if (fileSize == 0)
            continue;

        if (fileSize < MAX_SMALL_TASK_SIZE)
        {
            FileTask ft;
            ft.fileName = fileName;
            ft.beginPos = 0;
            ft.dataSize = fileSize;
            smallFileTasks.push_back(ft);
            smallTotalSize += fileSize;

            if (smallTotalSize >= MAX_SMALL_TASK_SIZE)
            {
                FileTask merged = smallFileTasks[0];
                merged.dataSize = smallTotalSize;
                tasks.emplace_back(merged, merged.dataSize);
                smallFileTasks.clear();
                smallTotalSize = 0;
            }
        }
        else
        {
            long pos = 0;
            while (pos < fileSize)
            {
                int slice = min(BIG_TASK_SLICE, (int)(fileSize - pos));
                FileTask ft;
                ft.fileName = fileName;
                ft.beginPos = pos;
                ft.dataSize = slice;
                tasks.emplace_back(ft, slice);
                pos += slice;
            }
        }
    }

    if (!smallFileTasks.empty())
    {
        FileTask merged = smallFileTasks[0];
        merged.dataSize = smallTotalSize;
        tasks.emplace_back(merged, merged.dataSize);
    }

    cout << "任务分解完成，共 " << tasks.size() << " 个子任务\n";
    return tasks;
}

// 执行单个任务（参数仍为MyServerNode引用，无需修改）
void executeTask(MyServerNode &node, const Task &task, const string &searchText)
{
    try
    {
        shared_ptr<TSocket> socket(new TSocket(node.ip, node.port));
        shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        TextSearchClient client(protocol);

        transport->open();
        client.ping();

        SearchResult result;
        client.searchText(result, task, searchText);

        {
            lock_guard<mutex> lock(g_resultMutex);
            g_allResults.push_back(result);
        }

        {
            lock_guard<mutex> lock(node.nodeMutex);
            node.currentTasks--;
        }

        transport->close();
    }
    catch (TException &tx)
    {
        cerr << "[客户端错误] " << tx.what() << endl;
        lock_guard<mutex> lock(node.nodeMutex);
        node.currentTasks--;
    }
}

// 基础调度（参数改为vector<unique_ptr<MyServerNode>>&）
void scheduleTasksBasic(vector<unique_ptr<MyServerNode>> &nodes, vector<Task> &tasks, const string &searchText)
{
    vector<thread> threads;
    const int MAX_PARALLEL = 2;

    for (auto &task : tasks)
    {
        bool assigned = false;
        while (!assigned)
        {
            for (auto &node_ptr : nodes) // node_ptr是unique_ptr<MyServerNode>
            {
                lock_guard<mutex> lock(node_ptr->nodeMutex); // 访问成员用->
                if (node_ptr->currentTasks < MAX_PARALLEL)
                {
                    node_ptr->currentTasks++;
                    // 传递对象的引用（*node_ptr）给executeTask
                    threads.emplace_back(executeTask, ref(*node_ptr), ref(task), ref(searchText));
                    assigned = true;
                    break;
                }
            }
            if (!assigned)
                this_thread::sleep_for(chrono::milliseconds(10));
        }
    }

    for (auto &t : threads)
        t.join();
}

// 改进调度：大文件优先+最轻载节点（参数改为vector<unique_ptr<MyServerNode>>&）
void scheduleTasksImproved(vector<unique_ptr<MyServerNode>> &nodes, vector<Task> &tasks, const string &searchText)
{
    vector<thread> threads;
    const int MAX_PARALLEL = 2;

    // 按任务大小降序排序（大任务优先）
    sort(tasks.begin(), tasks.end(), [](auto &a, auto &b)
         { return a.taskSize > b.taskSize; });

    for (auto &task : tasks)
    {
        bool assigned = false;
        while (!assigned)
        {
            unique_ptr<MyServerNode> *target = nullptr; // 指向unique_ptr的指针
            int minLoad = INT_MAX;

            // 找当前负载最小的节点
            for (auto &node_ptr : nodes)
            {
                lock_guard<mutex> lock(node_ptr->nodeMutex);
                if (node_ptr->currentTasks < MAX_PARALLEL &&
                    node_ptr->currentTasks < minLoad)
                {
                    minLoad = node_ptr->currentTasks;
                    target = &node_ptr; // 取unique_ptr的地址
                }
            }

            if (target)
            {
                lock_guard<mutex> lock((*target)->nodeMutex); // 解引用访问mutex
                (*target)->currentTasks++;
                // 传递MyServerNode对象的引用（**target）
                threads.emplace_back(executeTask, ref(**target), ref(task), ref(searchText));
                assigned = true;
            }
            else
            {
                // 所有节点都满载，等待10ms重试
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }

    for (auto &t : threads)
        t.join();
}

// 打印检索结果
void printResults(const string &text)
{
    int total = 0;
    cout << "\n=== 检索结果（\"" << text << "\"） ===\n";

    for (auto &r : g_allResults)
    {
        total += r.matchedNum;
        for (auto &m : r.matchLines)
        {
            cout << "行号: " << m.lineNum
                 << " 匹配位置: " << m.matchPos
                 << " 内容: " << m.lineContent << endl;
        }
    }

    cout << "总匹配次数：" << total << endl;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        cerr << "用法：./client <text> <basic|improved>\n";
        return 1;
    }

    string searchText = argv[1];
    string mode = argv[2];

    // 待检索的文件列表
    vector<string> files = {
        "./data/file2.txt",
        "./data/rand_file_001",
        "./data/rand_file_002",
        "./data/rand_file_003",
        "./data/rand_file_004",
        "./data/rand_file_005",
        "./data/rand_file_006",
        "./data/rand_file_007",
        "./data/rand_file_008",
        "./data/rand_file_009",
        "./data/rand_file_010",
        "./data/rand_file_011",
        "./data/rand_file_012",
        "./data/file3.txt",
        "./data/d.txt",
        "./data/c.txt",
        "./data/b.txt",
        "./data/bigfile.txt"};
    vector<unique_ptr<MyServerNode>> nodes;
    // 用emplace_back直接构造+移动，避免拷贝
    // nodes.emplace_back(make_unique<MyServerNode>("127.0.0.1", 9090));
    // nodes.emplace_back(make_unique<MyServerNode>("127.0.0.1", 9091));
    // nodes.emplace_back(make_unique<MyServerNode>("127.0.0.1", 9092));
    // 假设你有三台机器，IP 分别如下
    nodes.emplace_back(make_unique<MyServerNode>("127.0.0.1", 9090)); // 我自己的
    // nodes.emplace_back(make_unique<MyServerNode>("127.0.0.1", 9091)); // 我自己的
    nodes.emplace_back(make_unique<MyServerNode>("172.20.10.2", 9090)); // zzx
    nodes.emplace_back(make_unique<MyServerNode>("172.20.10.4", 9090)); // tj

    // 分解任务
    auto tasks = splitTasks(files);
    auto start = chrono::high_resolution_clock::now();

    // 执行调度
    if (mode == "basic")
        scheduleTasksBasic(nodes, tasks, searchText);
    else if (mode == "improved")
        scheduleTasksImproved(nodes, tasks, searchText);
    else
    {
        cerr << "无效模式！可选：basic / improved\n";
        return 1;
    }

    // 计算耗时
    auto end = chrono::high_resolution_clock::now();
    cout << "\n总耗时："
         << chrono::duration<double>(end - start).count() * 50
         << " 秒\n";

    // 打印结果
    printResults(searchText);
    return 0;
}