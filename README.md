# Large-Scale Text Processing & Parallel Computing System

A Linux-based large-scale text processing system implemented in C/C++, focusing on parallel processing, client-server communication, and performance optimization.

## Overview

This project explores how large-scale text retrieval workloads can be processed efficiently on Linux using multi-process and multi-threaded techniques.

The system adopts a client-server architecture. The client submits text processing requests, while the server manages request processing and performs parallel text retrieval.

## Architecture

```text
Client
  |
  | Request
  v
Server
  |
  +----------------------+
  |                      |
  v                      v
Process Pool         Thread Pool
  |                      |
  +----------+-----------+
             |
             v
      Text Processing
             |
             v
        Search Results
             |
             v
           Client
Key Features
Client-server architecture implemented with C/C++.
Linux-based development and execution environment.
Multi-process and multi-threaded approaches for parallel text processing.
Process/thread pool design to reduce repeated process creation overhead.
Large-scale text generation and retrieval workloads for performance evaluation.
Makefile-based compilation and modular project organization.
Performance analysis using execution time and system-level observations.
Technical Stack
Languages: C, C++
Operating System: Linux
Concurrency: Processes, threads, process pool, thread pool
System Programming: Linux system calls, IPC, process/thread management
Build: Makefile
Performance Analysis: execution-time measurement and workload testing
My Contribution
Designed and implemented core components of the large-scale text processing system.
Implemented parallel processing mechanisms using processes and threads.
Developed and tested process-pool and thread-pool approaches for text retrieval workloads.
Conducted performance testing and analyzed the impact of different parallelization strategies.
Participated in system debugging and Linux-based performance analysis.
Documentation

Project presentation and technical analysis are available in:

docs/linux汇报.pdf

Project Context

This project was completed as a systems and parallel-computing practice project, providing hands-on experience with Linux system programming, concurrency, large-scale data processing, and performance optimization.