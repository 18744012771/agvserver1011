#ifndef USERLOGMANAGER_H
#define USERLOGMANAGER_H

#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include "util/common.h"
#include "utils/noncopyable.h"
#include "network/protocol.h"
#include "network/session.h"

class UserLogManager;
using UserLogManagerPtr = std::shared_ptr<UserLogManager>;


//用户日志管理器，所有的用户日志，需要记录数据库
//对日志要发布给订阅日志的用户

class UserLogManager : public boost::noncopyable, public std::enable_shared_from_this<UserLogManager>
{
public:
    static UserLogManagerPtr getInstance()
    {
        static UserLogManagerPtr m_inst = UserLogManagerPtr(new UserLogManager());
        return m_inst;
    }

    void init();

    void push(const std::string &s);

    void interLogDuring(SessionPtr conn, const Json::Value &request);

private:
    UserLogManager();

    void checkTable();

    boost::lockfree::queue<USER_LOG> logQueue;
};

#endif // USERLOGMANAGER_H
