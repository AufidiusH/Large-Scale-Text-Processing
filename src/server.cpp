#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h> // 用于 getcwd
#include <sys/stat.h>

#include "gen-cpp/TextSearch.h"

// Thrift 0.19 头文件
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransport.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

class TextSearchHandler : virtual public TextSearchIf
{
public:
    TextSearchHandler() {}

    void ping() override
    {
        std::cout << "[服务端] 收到心跳检测" << std::endl;
    }

    void searchText(SearchResult &_return, const FileTask &task, const std::string &text) override
    {
        _return.matchedNum = 0;

        // ===== 调试信息 =====
        std::cout << "\n========== 收到搜索任务 ==========" << std::endl;
        std::cout << "文件名: " << task.fileName << std::endl;
        std::cout << "起始位置: " << task.beginPos << std::endl;
        std::cout << "数据大小: " << task.dataSize << std::endl;
        std::cout << "搜索文本: " << text << std::endl;

        // 打印当前工作目录
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            std::cout << "当前工作目录: " << cwd << std::endl;
        }

        // ===== 打开文件 =====
        std::ifstream file(task.fileName, std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "[错误] 无法打开文件: " << task.fileName << std::endl;

            // 检查文件是否存在
            struct stat buffer;
            if (stat(task.fileName.c_str(), &buffer) == 0)
            {
                std::cerr << "  文件存在但无法打开，可能是权限问题" << std::endl;
            }
            else
            {
                std::cerr << "  文件不存在" << std::endl;
            }
            return;
        }

        std::cout << "[成功] 文件已打开" << std::endl;

        // ===== 定位并读取指定区域 =====
        file.seekg(task.beginPos);
        std::string buffer(task.dataSize, '\0');
        file.read(&buffer[0], task.dataSize);
        std::streamsize bytesRead = file.gcount();
        file.close();

        std::cout << "实际读取字节数: " << bytesRead << std::endl;

        if (bytesRead == 0)
        {
            std::cerr << "[警告] 读取了0字节数据" << std::endl;
            return;
        }

        // ===== 按行分割并搜索 =====
        std::vector<std::string> lines;
        std::istringstream stream(buffer);
        std::string line;

        // 逐行读取
        while (std::getline(stream, line))
        {
            lines.push_back(line);
        }

        std::cout << "分割后的行数: " << lines.size() << std::endl;

        int lineNum = 1;
        for (const auto &line : lines)
        {
            // 跳过空行
            if (line.empty())
            {
                lineNum++;
                continue;
            }

            // 在当前行中查找所有匹配
            size_t pos = 0;
            while ((pos = line.find(text, pos)) != std::string::npos)
            {
                std::cout << "  ✓ 第" << lineNum << "行找到匹配，位置: " << pos << std::endl;

                MatchLine ml;
                ml.lineNum = lineNum;
                ml.matchPos = pos;
                ml.lineContent = line;
                _return.matchLines.push_back(ml);
                _return.matchedNum++;

                pos += text.length(); // 继续查找下一个匹配
            }
            lineNum++;
        }

        std::cout << "总匹配数: " << _return.matchedNum << std::endl;
        std::cout << "================================\n"
                  << std::endl;
    }
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "用法：./server <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    // 使用 Thrift 0.19 推荐的 ThreadedServer
    auto handler = std::make_shared<TextSearchHandler>();
    auto processor = std::make_shared<TextSearchProcessor>(handler);
    auto serverTransport = std::make_shared<TServerSocket>(port);
    auto transportFactory = std::make_shared<TBufferedTransportFactory>();
    auto protocolFactory = std::make_shared<TBinaryProtocolFactory>();

    TThreadedServer server(processor,
                           serverTransport,
                           transportFactory,
                           protocolFactory);

    std::cout << "[服务端] 启动成功，端口: " << port << std::endl;

    server.serve();
    return 0;
}