#ifndef TASKMAKER_H
#define TASKMAKER_H

#include "network/session.h"
#include "network/protocol.h"

class TaskMaker
{
public:
    static  TaskMaker* getInstance();
    virtual ~TaskMaker();

    virtual void init() = 0;

    virtual void makeTask(SessionPtr conn, const Json::Value &request) = 0;

    //群创接口//非群创不需要重写
    virtual void makeTask(std::string from ,std::string to,std::string dispatch_id,int ceid,std::string line_id, int agv_id, int all_floor_info);
protected:
    TaskMaker();
};

#endif // TASKMAKER_H
