#ifndef MSGPROCESS_H
#define MSGPROCESS_H

#include <memory>
#include <list>
#include <mutex>
#include <boost/asio.hpp>
#include "network/protocol.h"
#include "network/session.h"

class MsgProcess;

using MsgProcessPtr = std::shared_ptr<MsgProcess>;

typedef enum{
    ENUM_NOTIFY_ALL_TYPE_MAP_UPDATE = 0,
    ENUM_NOTIFY_ALL_TYPE_ERROR,
}ENUM_NOTIFY_ALL_TYPE;

class MsgProcess : public boost::noncopyable,public std::enable_shared_from_this<MsgProcess>
{
public:

    static MsgProcessPtr getInstance(){
        static MsgProcessPtr m_inst = MsgProcessPtr(new MsgProcess());
        return m_inst;
    }

    bool init();

    void removeSubSession(int session);

    //用户断开连接导致 用户退出登录
    void sessionLogout(int user_id);

    //进来一个消息,分配给一个线程去处理它
    void processOneMsg(const Json::Value &request,SessionPtr session);

    //发布一个日志消息
    void publishOneLog(USER_LOG log);

    //通知所有用户的事件
    void notifyAll(ENUM_NOTIFY_ALL_TYPE type);

    //发生错误，需要告知
    void errorOccur(int code,std::string msg,bool needConfirm);

    //用户接口
    void interAddSubAgvPosition(SessionPtr conn, const Json::Value &request);
    void interAddSubAgvStatus(SessionPtr conn, const Json::Value &request);
    void interAddSubTask(SessionPtr conn, const Json::Value &request);
    void interAddSubLog(SessionPtr conn, const Json::Value &request);
    void interRemoveSubAgvPosition(SessionPtr conn, const Json::Value &request);
    void interRemoveSubAgvStatus(SessionPtr conn,const Json::Value &request);
    void interRemoveSubTask(SessionPtr conn, const Json::Value &request);
    void interRemoveSubLog(SessionPtr conn, const Json::Value &request);

    void onSessionClosed(int id);

    void addSubAgvPosition(int id);
    void addSubAgvStatus(int id);
    void addSubTask(int id);
    void addSubLog(int id);

    void removeSubAgvPosition(int id);
    void removeSubAgvStatus(int id);
    void removeSubTask(int id);
    void removeSubLog(int id);
private:

    void publisher_agv_position();

    void publisher_agv_status();

    void publisher_task();

    MsgProcess();

    std::mutex psMtx;
    std::list<int> agvPositionSubers;

    std::mutex ssMtx;
    std::list<int> agvStatusSubers;

    std::mutex tsMtx;
    std::list<int> taskSubers;

    std::mutex lsMtx;
    std::list<int> logSubers;

    std::mutex errorMtx;
    int error_code;
    std::string error_info;
    bool needConfirm;
};



#endif // MSGPROCESS_H
