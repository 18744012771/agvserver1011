#ifndef SessionBuffer_H
#define SessionBuffer_H

#include <vector>
#include <memory>

#define QYH_BUFFER_DEFAULT_SIZE 1500

class SessionBuffer
{
public:
    SessionBuffer(int size = QYH_BUFFER_DEFAULT_SIZE);
    SessionBuffer(const SessionBuffer &b);
    SessionBuffer(const std::vector<char> &b);
    SessionBuffer(const char *data,int len);
    ~SessionBuffer() = default;

    std::unique_ptr<SessionBuffer> clone();

    uint32_t size(); // Size of internal vector

    int find(char key,int start = 0);

    std::string substr(int start = 0,int len = -1);

    //如果不够一个int，返回一个-1
    int getInt32(int start = 0) const;

    const char *data(int start) const;

    void append(const char *data,int len);

    SessionBuffer &operator=(const SessionBuffer &other);

    SessionBuffer &operator+=(const SessionBuffer &other);

    bool operator == (const SessionBuffer &other);

    const std::vector<char> &buffer() const { return buf; }

    void clear();

    int length() const { return (int)buf.size(); }

    bool empty() const { return buf.empty(); }

    void removeFront(int len);

public:
    std::vector<char> buf;

};

#endif // SessionBuffer_H
