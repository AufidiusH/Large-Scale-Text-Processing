# Large-Scale Text Processing & Parallel Computing System

A multi-machine parallel text retrieval system implemented with **C/C++** and **Apache Thrift RPC**.

## Features

* Distributed text retrieval across multiple server nodes
* Large-file slicing and small-file aggregation
* Dynamic task scheduling and workload balancing
* Concurrent RPC-based task execution
* Performance optimization and evaluation on a **100 GB** dataset

## Performance

| Strategy            | Average Time |
| ------------------- | -----------: |
| Basic Scheduling    |      402.2 s |
| Improved Scheduling |      255.6 s |

The improved scheduling strategy reduced the average retrieval time by approximately **36.4%**.

## Tech Stack

**C/C++ · Linux · Apache Thrift · TCP · Multithreading · Distributed Computing**

## Structure

```text
Large-Scale-Text-Processing/
├── README.md
├── src/
│   ├── client.cpp
│   └── server.cpp
└── docs/
    └── linux汇报.pdf
```
