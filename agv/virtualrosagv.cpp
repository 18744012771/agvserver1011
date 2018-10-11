#include "virtualrosagv.h"
#include "mapmap/onemap.h"
#include "mapmap/mapmanager.h"
#include "agv/virtualagv.h"
#include "task/agvtask.h"
#include "util/bezierarc.h"
#include <limits>

#define _USE_MATH_DEFINES
#include "math.h"
#include <float.h>


VirtualRosAgv::VirtualRosAgv(int id,std::string name)
    :VirtualAgv(id,name)
{
    lastStationOdometer=0;
    isPaused=(false);

}


VirtualRosAgv::~VirtualRosAgv()
{

}

void VirtualRosAgv::init()
{
    //do nothing
}

bool VirtualRosAgv::pause()
{
    isPaused = true;
    return true;
}

bool VirtualRosAgv::resume()
{
    isPaused = false;
    return true;
}

void VirtualRosAgv::excutePath(std::vector<int> lines)
{
    auto mapmanagerptr = MapManager::getInstance();
    isStop = false;
    stationMtx.lock();
    excutespaths = lines;
    excutestations.clear();
    for (auto line : lines) {
        MapPath *path = mapmanagerptr->getPathById(line);
        if(path == nullptr)continue;
        int endId = path->getEnd();
        excutestations.push_back(endId);
    }
    stationMtx.unlock();

    int next = 0;//下一个要去的位置
    MapPoint *currentPoint = mapmanagerptr->getPointById(nowStation);
    MapPoint *nextPoint = mapmanagerptr->getPointById(nextStation);
    MapPoint *lastPoint = mapmanagerptr->getPointById(lastStation);
    if(currentPoint==nullptr && lastPoint!=nullptr){
        currentPoint = lastPoint;
    }
    if(currentPoint == nullptr &&lastPoint==nullptr && nextPoint!=nullptr){
        currentPoint = nextPoint;
    }
    if(currentPoint == nullptr){
        combined_logger->debug("agv position lost! can not excute path");
        return ;
    }
    bool in_elevator = currentPoint->getMapChange();
    for(unsigned int i=0;i<excutestations.size();++i)
    {
        if(currentTask!=nullptr && currentTask->getIsCancel())break;//任务取消
        if(status == AGV_STATUS_HANDING)break;//手动控制
        if(status == AGV_STATUS_ERROR)break;//发生错误

  //      floor = mapmanagerptr->getFloor(nowStation);

        int now = excutestations[i];//接下来要去的位置
        if(i+1<excutestations.size())
            next = excutestations[i+1];
        else
            next = 0;

        if(next == 0){
            //没有下一站了
            goStation(now,true);
            continue;
        }

        MapPoint *nowPoint = mapmanagerptr->getPointById(now);
        MapPoint *nextPoint = mapmanagerptr->getPointById(next);
        if(nowPoint == nullptr)continue;
        if(nextPoint == nullptr){
            goStation(now, true);
            continue;
        }

        //还有下一站。
        //分类讨论
        if(!nowPoint->getMapChange() && !nextPoint->getMapChange())
        {
            //两个都不是地图切换点
            goStation(now,false);
            in_elevator = false;
        }
        else if(!in_elevator)
        {
            goStation(now, true);
            in_elevator = true;
        }
//        else
//        {
//            callMapChange(now);
//        }
    }
    //if(currentTask!=nullptr && currentTask->getIsCancel())cancelTask();
    if(status == AGV_STATUS_HANDING || status == AGV_STATUS_ERROR)cancelTask();
}

void VirtualRosAgv::cancelTask()
{
    if(currentTask!=nullptr)
        currentTask->cancel();
    //是否保存到数据库呢?
    stop();
}

void VirtualRosAgv::goStation(int station, bool stop)
{
    //BlockManagerPtr blockmanagerptr = BlockManager::getInstance();
    MapManagerPtr mapmanagerptr = MapManager::getInstance();
    //看是否是写特殊点
    MapPoint *endPoint = mapmanagerptr->getPointById(station);
    if(endPoint == nullptr)return ;

    //获取当前线路和当前站点
    MapPoint *startPoint = nullptr;
    if(nowStation > 0){
        startPoint = mapmanagerptr->getPointById(nowStation);
    }else{
        startPoint = mapmanagerptr->getPointById(lastStation);
    }

    if(startPoint==nullptr){
        //初始位置未知！
        return ;
    }

    //获取当前走的线路
    MapPath *path = mapmanagerptr->getMapPathByStartEnd(startPoint->getId(),station);
    if(path==nullptr){
        //根本不存在该线路
        return ;
    }

    //计算线路长度[以图像上的长度为长度]
    PointF a(startPoint->getX(),startPoint->getY());
    PointF b(path->getP1x(),path->getP1y());
    PointF c(path->getP2x(),path->getP2y());
    PointF d(endPoint->getX(),endPoint->getY());

    double currentT = 0.;
    double path_length = 100;
    if(path->getPathType() == MapPath::Map_Path_Type_Line){
        path_length = sqrt((endPoint->getY()-startPoint->getY())*(endPoint->getY()-startPoint->getY())+(endPoint->getX()-startPoint->getX())*(endPoint->getX()-startPoint->getX()));
        double minDistance = DBL_MAX;
        for (double tt = 0.0; tt <= 1.0; tt += 0.01) {
            double distance = getDistance(PointF(startPoint->getX()+(endPoint->getX()-startPoint->getX())*tt,startPoint->getY()+(endPoint->getY()-startPoint->getY())*tt), PointF(x, y));
            if (distance<minDistance) {
                minDistance = distance;
                currentT = tt;
            }
        }
    }else if(path->getPathType() == MapPath::Map_Path_Type_Quadratic_Bezier){
        path_length = BezierArc::BezierArcLength(a,b,d);
        //获取当前位置在曲线上的位置
        double minDistance = DBL_MAX;
        for(double tt = 0.0;tt<=1.0;tt+=0.01){
            BezierArc::POSITION_POSE pp = BezierArc::BezierArcPoint(a,b,d,tt);
            double distance = getDistance(pp.pos,PointF(x,y));
            if(distance<minDistance){
                minDistance = distance;
                currentT = tt;
            }
        }
    }else if(path->getPathType() == MapPath::Map_Path_Type_Cubic_Bezier){
        path_length = BezierArc::BezierArcLength(a,b,c,d);
        //计算当前位置到曲线上最近的点的距离
        //获取当前位置在曲线上的位置
        double minDistance = DBL_MAX;
        for(double tt = 0.0;tt<=1.0;tt+=0.01){
            BezierArc::POSITION_POSE pp = BezierArc::BezierArcPoint(a,b,c,d,tt);
            double distance = getDistance(pp.pos,PointF(x,y));
            if(distance<minDistance){
                minDistance = distance;
                currentT = tt;
            }
        }
    }

    if(fabs(path_length) < 0.1)
    {
        path_length = 100;
        combined_logger->info("path_length=0 error");
    }

//    while(!g_quit && currentTask!=nullptr && !currentTask->getIsCancel()){
//        //can start the path?
//        bool canGo = true;
//        auto bs = mapmanagerptr->getBlocks(path->getId());
//        if(!blockmanagerptr->tryAddBlockOccu(bs,getId(),path->getId())){
//            canGo = false;
//        }

//        if(canGo){
//            auto bs2 = mapmanagerptr->getBlocks(path->getEnd());
//            if(!blockmanagerptr->tryAddBlockOccu(bs2,getId(),path->getEnd()))
//            {
//                canGo = false;
//            }
//        }

//        if(canGo)break;
//        usleep(500000);
//    }
    if(g_quit || currentTask == nullptr || currentTask->getIsCancel())return ;

    //进行模拟 移动位置
    bool firstMove = true;
    while(true){
        if(isStop)break;
        if(isPaused){
            Sleep(500);
            continue;
        }
        //1.向目标前进100ms的距离 假设每次前进10 //3.重新计算当前位置
        if(path->getPathType() == MapPath::Map_Path_Type_Line){
            currentT += 10.0 / path_length;
            //前移10
            x = startPoint->getX()+(endPoint->getX()-startPoint->getX()) * currentT;
            y = startPoint->getY() + (endPoint->getY() - startPoint->getY()) * currentT;
            if(path->getSpeed()<0){
                theta =180-atan2(endPoint->getY()-y,endPoint->getX()-x)*180/M_PI;
            }else{
                theta = atan2(endPoint->getY()-y,endPoint->getX()-x)*180/M_PI;
            }
        }else if(path->getPathType() == MapPath::Map_Path_Type_Quadratic_Bezier){
            //前移10
            currentT += 10.0/path_length;
            if(currentT<0)currentT = 0.;
            if(currentT>1)currentT = 1.;
            BezierArc::POSITION_POSE pp = BezierArc::BezierArcPoint(a,b,d,currentT);
            x = pp.pos.x();
            y = pp.pos.y();
            if(path->getSpeed()<0){
                theta = 180 -pp.angle;
            }else{
                theta = pp.angle;
            }
        }else if(path->getPathType() == MapPath::Map_Path_Type_Cubic_Bezier){
            //前移10
            currentT += 10.0/path_length;
            if(currentT<0)currentT = 0.;
            if(currentT>1)currentT = 1.;
            BezierArc::POSITION_POSE pp = BezierArc::BezierArcPoint(a,b,c,d,currentT);
            x = pp.pos.x();
            y = pp.pos.y();
            if(path->getSpeed()<0){
                theta =180 -pp.angle;
            }else{
                theta = pp.angle;
            }
        }
        //2.初次移动，调用离开上一站
        if(firstMove){
            if(nowStation>0){
                onLeaveStation(nowStation);
            }
            firstMove = false;
        }

        //4.判断是否到达下一站 如果到达，break
        if(currentT>=1.0){
            onArriveStation(station);
            lastStation = nowStation;
            nowStation = station;
           // floor = mapmanagerptr->getFloor(station);
            break;
        }
        Sleep(500);
    }


}

void VirtualRosAgv::stop()
{
    isStop = true;
}

//void VirtualRosAgv::callMapChange(int station)
//{
//    //模拟电梯运行，这里只做等待即可
//    Sleep(5000);
//    lastStation = nowStation;
//    nowStation = station;
//    floor = MapManager::getInstance()->getFloor(station);
//    combined_logger->debug("go elvator:current floor={0}", floor);
//}
void VirtualRosAgv::onTaskStart(AgvTaskPtr _task)
{
    if(_task != nullptr)
    {
        status = Agv::AGV_STATUS_TASKING;
    }
}

void VirtualRosAgv::onTaskCanceled(AgvTaskPtr _task){
    auto mapmanagerptr = MapManager::getInstance();
    if(currentTask!=nullptr){
        auto nodes = currentTask->getTaskNodes();
        auto index = currentTask->getDoingIndex();
        if(index<nodes.size())
        {
            auto node = nodes[index];
            mapmanagerptr->freeStation(node->getStation(),shared_from_this());
            auto paths = currentTask->getPath();
            for(auto p:paths){
                mapmanagerptr->freeLine(p,shared_from_this());
            }
        }
    }
}
