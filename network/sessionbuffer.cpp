#include "sessionbuffer.h"
#include <iostream>
#include <memory.h>







SessionBuffer::SessionBuffer(const SessionBuffer &b)
{
    //std::cout<<"sessionbuffer";
    buf = b.buf;
}

SessionBuffer::SessionBuffer(int size){
    buf.reserve(size);
    clear();
}


SessionBuffer::SessionBuffer(const char *data,int len)
{
    if (data == NULL) {
        buf.reserve(len);
        clear();
    } else { // Consume the provided array
        buf.reserve(len);
        clear();
        append(data, len);
    }
}

SessionBuffer::SessionBuffer(const std::vector<char> &b)
{
    buf = b;
}

std::unique_ptr<SessionBuffer> SessionBuffer::clone()
{
    std::unique_ptr<SessionBuffer> ret = std::make_unique<SessionBuffer>(buf);
    return ret;
}

uint32_t SessionBuffer::size()
{
    return length();
}

int SessionBuffer::find(char key,int start)
{
    int ret = -1;
    uint32_t len = buf.size();
    for (uint32_t i = start; i < len; i++) {
        if (buf[i] == key) {
            ret = (int) i;
            break;
        }
    }
    return ret;
}

std::string SessionBuffer::substr(int start, int len)
{
    std::string ret;
    if(start<size()){
        if(len<0){
            ret = std::string(data(start));
        }else{
            ret = std::string(data(start),len);
        }
    }
    return ret;
}

int SessionBuffer::getInt32(int start) const
{
    int ret;
    if(start + sizeof(int32_t)>buf.size())return -1;

    memcpy(&ret,&buf[start],sizeof(int32_t));
    return ret;

    //    return *((int32_t*) &buf[start]);
}

const char *SessionBuffer::data(int start) const
{
    if(empty()) return nullptr;
    if(start>buf.size())return nullptr;
    return &buf[start];
}

void SessionBuffer::append(const char *data,int len)
{
    if(data == NULL || len == 0) return;
    buf.resize(length() + len,0);
    memcpy(&buf[0] + length() - len,data,len);
}

SessionBuffer &SessionBuffer::operator=(const SessionBuffer &other)
{
    buf = other.buf;
    return *this;
}

SessionBuffer &SessionBuffer::operator+=(const SessionBuffer &other)
{
    if(!other.empty()){
        buf.insert(buf.end(), other.buf.begin(), other.buf.end());
    }
    return *this;
}

bool SessionBuffer::operator == (const SessionBuffer &other)
{
    return buf == other.buf;
}

void SessionBuffer::clear()
{
    buf.clear();
}

void SessionBuffer::removeFront(int len)
{
    if(len<=0)return ;
    if(len > length())
    {
        clear();
    }else{
        buf.erase(buf.begin(),buf.begin()+len);
    }
}
