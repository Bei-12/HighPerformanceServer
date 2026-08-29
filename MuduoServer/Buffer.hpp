// 追加数据 消费数据
#include <cstring>
#include <iostream>
#include <string>

using namespace std;

class Buffer
{
public:
    // 添加数据
    void Append(const char* data, size_t len)// 数据起始地址 + 实际收到的字节数
    {
        String_.append(data, len);
    }

    void Append(string buffer)
    {
        String_ += buffer;
    }

    // 消费一部分数据
    void Retrieve(int num)
    {
        if(num <= (int)Size())
            String_.erase(0, num);
        else
            return;
    }

    // 清空已处理数据
    void RetrieveAll()
    {
        String_.clear();
    }

    // 是否为空
    bool Empty()
    {
        return String_.empty();
    }

    string GetString()
    {
        return String_;
    }

    size_t Size()
    {
        return String_.size();
    }


private:
    string String_;
};