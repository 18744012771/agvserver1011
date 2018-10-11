
#include <thread>

#include "agv.h"
#include "mapmap/mapmanager.h"
#include "task/agvtask.h"
#include "network/tcpclient.h"
#include "user/userlogmanager.h"
#include "user/msgprocess.h"
#include "mapmap/mapmanager.h"



Agv::Agv(int _id, std::string _name, std::string _ip, int _port) :
    currentTask(nullptr),
    id(_id),
    name(_name),
    ip(_ip),
    port(_port),
    lastStation(0),
    nowStation(0),
    nextStation(0),
    x(0),
    y(0),
    theta(0)
    //floor(-1)
{
}

Agv::~Agv()
{
}

void Agv::init()
{
}

void Agv::setPosition(int _lastStation, int _nowStation, int _nextStation) {
    lastStation = _lastStation;
    nowStation = _nowStation;
    nextStation = _nextStation;
    auto mapmanagerptr = MapManager::getInstance();
    if (nowStation > 0) {
        onArriveStation(nowStation);
        mapmanagerptr->addOccuStation(nowStation,shared_from_this());
    }else if(lastStation>0 && nextStation>0){
        auto line = mapmanagerptr->getPathByStartEnd(lastStation,nextStation);
        if(line!=nullptr){
            mapmanagerptr->addOccuLine(line->getId(),shared_from_this());
        }
    }
}

//到达后是否停下，如果不停下，就是不减速。
//是一个阻塞的函数
void Agv::goStation(int station, bool stop)
{
}

void Agv::stop()
{
}

void Agv::onArriveStation(int station)
{
    auto mapmanagerptr = MapManager::getInstance();

    MapPoint *point = mapmanagerptr->getPointById(station);
    if(point == nullptr)return ;
    combined_logger->info("agv id:{0} arrive station:{1}",getId(),point->getName());
//    ///yuan
//    //到达的时候占用当前station
//    mapmanagerptr->addOccuStation(station,shared_from_this());

//    ///yuan



//    x = point->getX();
//    y = point->getY();

    if(station>0){



        if(nowStation>0){//?????
            lastStation = nowStation;
            //free last station occu
            //????
            mapmanagerptr->freeStation(lastStation,shared_from_this());

        }

        nowStation = station;
        stationMtx.lock();
        for (int i = 0; i < excutestations.size(); ++i) {
            if (excutestations[i] == station) {
                if (i + 1 < excutestations.size()) {
                    nextStation = excutestations[i + 1];
                }
                else{
                    nextStation = 0;
                }
                break;
            }
        }
        stationMtx.unlock();
    }
    //TODO:释放之前的线路和站点
    std::vector<MapPath *> paths;
    stationMtx.lock();
    for(auto line:excutespaths){
        MapPath *path = mapmanagerptr->getPathById(line);
        if(path == nullptr)continue;
        paths.push_back(path);
    }
    stationMtx.unlock();

    int findIndex = -1;
    for(int i=0;i<paths.size();++i){
        auto line = paths[i];
        if(line->getEnd() == station){
            findIndex = i;
        }
    }
    if(findIndex != -1){
        //之前的道路占用全部释放
        //之前的站点占用全部释放
        for(int i=0;i<=findIndex;++i){
            auto line = paths[i];
            int start = line->getStart();
            int end = line->getEnd();
            int lineId = line->getId();
            //free start station
            mapmanagerptr->freeStation(start,shared_from_this());


            //free end station
            if(i!=findIndex){
                mapmanagerptr->freeStation(end,shared_from_this());

            }

            //free last line
            mapmanagerptr->freeLine(lineId,shared_from_this());

        }
    }

    char buf[SQL_MAX_LENGTH];
    snprintf(buf, SQL_MAX_LENGTH, "update agv_agv set lastStation=%d,nowStation=%d,nextStation=%d  where id = %d;", lastStation, nowStation, nextStation, id);
    try {
        g_db.execDML(buf);
    }
    catch (CppSQLite3Exception e) {
        combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
    }
    catch (std::exception e) {
        combined_logger->error("sqlerr code:{0} ", e.what());
    }


}

void Agv::onLeaveStation(int stationid)
{
    auto mapmanagerptr = MapManager::getInstance();
    nowStation = 0;
    lastStation = stationid;

    auto point = mapmanagerptr->getPointById(stationid);
    if(point!=nullptr){
        combined_logger->info("agv id:{0} leave station:{1}",getId(),point->getName());
    }

    //释放这个站点的占用
    MapManager::getInstance()->freeStation(stationid,shared_from_this());

    bool b_getNextStation = false;
    stationMtx.lock();
    for(auto line:excutespaths){
        MapPath *path = mapmanagerptr->getPathById(line);
        if(path == nullptr)continue;
        if(path->getStart() == nowStation){
            nextStation = path->getEnd();
            ///yuan 占用nextline
            ///mapmanagerptr->addOccuLine(line,shared_from_this());
            /// yuan

            b_getNextStation = true;
            break;
        }
    }
    stationMtx.unlock();
    if(!b_getNextStation){
        nextStation = 0;
    }

    char buf[SQL_MAX_LENGTH];
    snprintf(buf, SQL_MAX_LENGTH, "update agv_agv set lastStation=%d,nowStation=%d,nextStation=%d  where id = %d;", lastStation, nowStation, nextStation,id);
    try {
        g_db.execDML(buf);
    }
    catch (CppSQLite3Exception e) {
        combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
    }
    catch (std::exception e) {
        combined_logger->error("sqlerr code:{0} ", e.what());
    }

}

void Agv::onError(int code, std::string msg)
{
    status = AGV_STATUS_ERROR;
    char sss[1024];
    snprintf(sss, 1024, "Agv id:%d occur error code:%d msg:%s", id, code, msg.c_str());
    std::string ss(sss);
    combined_logger->error(ss.c_str());
    UserLogManager::getInstance()->push(ss);
    MsgProcess::getInstance()->errorOccur(code, msg, true);
}

void Agv::onWarning(int code, std::string msg)
{
    char sss[1024];
    snprintf(sss, 1024, "Agv id:%d occur warning code:%d msg:%s", id, code, msg.c_str());
    std::string ss(sss);
    combined_logger->warn(ss.c_str());
    UserLogManager::getInstance()->push(ss);
    MsgProcess::getInstance()->errorOccur(code, msg, false);
}



void Agv::cancelTask()
{
    onTaskCanceled(currentTask);
}

void Agv::reconnect()
{
}

