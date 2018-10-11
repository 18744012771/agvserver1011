#include "mapmanager.h"
#include "sql/cppsqlite3.h"
#include "user/msgprocess.h"
#include "util/common.h"
#include "task/taskmanager.h"
#include "user/userlogmanager.h"
#include "util/base64.h"
//#include "blockmanager.h"
#include <algorithm>

MapManager::MapManager() :mapModifying(false)
{
}

void MapManager::checkTable()
{
    //检查表
    try {
        if (!g_db.tableExists("agv_station")) {
            g_db.execDML("create table agv_station(id INTEGER,name char(64),type INTEGER, x INTEGER,y INTEGER,realX INTEGER,realY INTEGER,realA INTEGER DEFAULT 0, labelXoffset INTEGER,labelYoffset INTEGER,mapChange BOOL,locked BOOL,ip char(64),port INTEGER,agvType INTEGER,lineId char(64));");
        }
        if (!g_db.tableExists("agv_line")) {
            g_db.execDML("create table agv_line(id INTEGER,name char(64),type INTEGER,start INTEGER,end INTEGER,p1x INTEGER,p1y INTEGER,p2x INTEGER,p2y INTEGER,length INTEGER,locked BOOL,speed DOUBLE DEFAULT 0.4);");
        }
        if (!g_db.tableExists("agv_bkg")) {
            g_db.execDML("create table agv_bkg(id INTEGER,name char(64),data blob,data_len INTEGER,x INTEGER,y INTEGER,width INTEGER,height INTEGER,filename char(512));");
        }

    }
    catch (CppSQLite3Exception &e) {
       combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
        return;
    }
    catch (std::exception e) {
       combined_logger->error("sqlerr code:{0}", e.what());
        return;
    }
}

//载入地图
bool MapManager::load()
{
    checkTable();
    //载入数据
    mapModifying = true;
    if (!loadFromDb())//加载db中地图信息
    {
        mapModifying = false;
        return false;
    }
    mapModifying = false;
    return true;
}

mapElement * MapManager::getMapElementById(int id)
{
    return g_onemap.getElementById(id);
}

mapElement *MapManager::getMapElementByName(std::string name)
{
    auto ae = g_onemap.getAllElement();
    for (auto e : ae) {
        if (e->getName() == name)return e;
    }
    return nullptr;
}

MapPath *MapManager::getMapPathByStartEnd(int start, int end)
{
    auto ae = g_onemap.getAllElement();
    for (auto e : ae) {
        if (e->getElementType() == mapElement::Map_Element_Type_Path) {
            MapPath *path = static_cast<MapPath *>(e);
            if (path->getStart() == start && path->getEnd() == end)return path;
        }
    }
    return nullptr;
}



//一个Agv占领一个站点
void MapManager::addOccuStation(int station, AgvPtr occuAgv)
{
    station_occuagv[station] = occuAgv->getId();
    combined_logger->debug("occu station:{0} agv:{1}", station, occuAgv->getId());
}

//线路的反向占用//这辆车行驶方向和线路方向相反
void MapManager::addOccuLine(int line, AgvPtr occuAgv)
{
    if(std::find(line_occuagvs[line].begin(), line_occuagvs[line].end(), occuAgv->getId()) == line_occuagvs[line].end())
    {
        line_occuagvs[line].push_back(occuAgv->getId());
    }
    combined_logger->debug("occupy line:{0} agv:{1}", line, occuAgv->getId());

}

//如果车辆占领该站点，释放
void MapManager::freeStation(int station, AgvPtr occuAgv)
{
    if (station_occuagv[station] == occuAgv->getId()) {
        station_occuagv[station] = 0;
    }
    combined_logger->debug("free station:{0} agv:{1}", station, occuAgv->getId());
}

//如果车辆在线路的占领表中，释放出去
void MapManager::freeLine(int line, AgvPtr occuAgv)
{
    for (auto itr = line_occuagvs[line].begin(); itr != line_occuagvs[line].end(); ) {
        if (*itr == occuAgv->getId()) {
            itr = line_occuagvs[line].erase(itr);
        }
        else {
            ++itr;
        }
    }
    combined_logger->info("free line:{0} agv:{1}", line, occuAgv->getId());
}



//保存到数据库
bool MapManager::save()
{
    try {
        checkTable();

        g_db.execDML("delete from agv_station;");

        g_db.execDML("delete from agv_line;");

        g_db.execDML("delete from agv_bkg;");

        g_db.execDML("delete from agv_floor;");

        g_db.execDML("delete from agv_block;");

        g_db.execDML("delete from agv_group;");

        g_db.execDML("begin transaction;");

        std::list<mapElement *> Elements = g_onemap.getAllElement();

        CppSQLite3Buffer bufSQL;

        for (auto Element : Elements) {
            if (Element->getElementType() == mapElement::Map_Element_Type_Point) {
                MapPoint *station = static_cast<MapPoint *>(Element);
                bufSQL.format("insert into agv_station(id,name,type, x,y,realX,realY,realA,labelXoffset,labelYoffset ,mapChange,locked,ip,port,agvType,lineId) values (%d, '%s',%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,'%s',%d,%d,'%s');", station->getId(), station->getName().c_str(), station->getPointType(), station->getX(), station->getY(),
                              station->getRealX(), station->getRealY(), station->getRealA(), station->getLabelXoffset(), station->getLabelYoffset(), station->getMapChange(), station->getLocked(), station->getIp().c_str(), station->getPort(), station->getAgvType(),station->getLineId().c_str());
                g_db.execDML(bufSQL);
            }
            else if (Element->getElementType() == mapElement::Map_Element_Type_Path) {
                MapPath *path = static_cast<MapPath *>(Element);
                bufSQL.format("insert into agv_line(id ,name,type ,start ,end ,p1x ,p1y ,p2x ,p2y ,length ,locked,speed) values (%d,'%s', %d,%d, %d, %d, %d, %d, %d, %d, %d,%.2f);", path->getId(), path->getName().c_str(), path->getPathType(), path->getStart(), path->getEnd(),
                              path->getP1x(), path->getP1y(), path->getP2x(), path->getP2y(), path->getLength(), path->getLocked(),path->getSpeed());
                g_db.execDML(bufSQL);
            }
            else if (Element->getElementType() == mapElement::Map_Element_Type_Background) {
                MapBackground *bkg = static_cast<MapBackground *>(Element);
                CppSQLite3Binary blob;
                blob.setBinary((unsigned char *)bkg->getImgData(), bkg->getImgDataLen());
                bufSQL.format("insert into agv_bkg(id ,name,data,data_len,x ,y ,width ,height ,filename) values (%d,'%s',%Q, %d,%d, %d, %d, %d,'%s');", bkg->getId(), bkg->getName().c_str(), blob.getEncoded(), bkg->getImgDataLen(), bkg->getX(), bkg->getY(), bkg->getWidth(), bkg->getHeight(), bkg->getFileName().c_str());
                g_db.execDML(bufSQL);
            }

        }

        g_db.execDML("commit transaction;");
    }
    catch (CppSQLite3Exception &e) {
       combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
        return false;
    }
    catch (std::exception e) {
       combined_logger->error("sqlerr code:{0}", e.what());
        return false;
    }
    return true;
}

//从数据库中载入地图
bool MapManager::loadFromDb()
{
    try {
        checkTable();

        CppSQLite3Table table_station = g_db.getTable("select id, name, x ,y ,type ,realX ,realY ,realA, labelXoffset ,labelYoffset ,mapChange ,locked ,ip ,port ,agvType ,lineId  from agv_station;");
        if (table_station.numRows() > 0 && table_station.numFields() != 16)
        {
           combined_logger->error("MapManager loadFromDb agv_station error!");
            return false;
        }
        for (int row = 0; row < table_station.numRows(); row++)
        {
            table_station.setRow(row);

            int id = atoi(table_station.fieldValue(0));
            std::string name = std::string(table_station.fieldValue(1));
            int x = atoi(table_station.fieldValue(2));
            int y = atoi(table_station.fieldValue(3));
            int type = atoi(table_station.fieldValue(4));
            int realX = atoi(table_station.fieldValue(5));
            int realY = atoi(table_station.fieldValue(6));
            int realA = atoi(table_station.fieldValue(7));
            int labeXoffset = atoi(table_station.fieldValue(8));
            int labelYoffset = atoi(table_station.fieldValue(9));
            bool mapchange = atoi(table_station.fieldValue(10)) == 1;
            bool locked = atoi(table_station.fieldValue(11)) == 1;
            std::string ip = std::string(table_station.fieldValue(12));
            int port = atoi(table_station.fieldValue(13));
            int agvType = atoi(table_station.fieldValue(14));
            std::string lineId = std::string(table_station.fieldValue(15));

            MapPoint::Map_Point_Type t = static_cast<MapPoint::Map_Point_Type>(type);
            MapPoint *point = new MapPoint(id, name, t, x, y, realX, realY, realA, labeXoffset, labelYoffset, mapchange, locked, ip, port, agvType, lineId);

            g_onemap.addElement(point);
        }

        CppSQLite3Table table_line = g_db.getTable("select id,name,type,start,end,p1x,p1y,p2x,p2y,length,locked,speed from agv_line;");
        if (table_line.numRows() > 0 && table_line.numFields() != 12)return false;
        for (int row = 0; row < table_line.numRows(); row++)
        {
            table_line.setRow(row);

            int id = atoi(table_line.fieldValue(0));
            std::string name = std::string(table_line.fieldValue(1));
            int type = atoi(table_line.fieldValue(2));
            int start = atoi(table_line.fieldValue(3));
            int end = atoi(table_line.fieldValue(4));
            int p1x = atoi(table_line.fieldValue(5));
            int p1y = atoi(table_line.fieldValue(6));
            int p2x = atoi(table_line.fieldValue(7));
            int p2y = atoi(table_line.fieldValue(8));
            int length = atoi(table_line.fieldValue(9));
            bool locked = atoi(table_line.fieldValue(10)) == 1;
            double speed = atof(table_line.fieldValue(11));

            MapPath::Map_Path_Type t = static_cast<MapPath::Map_Path_Type>(type);
            MapPath *path = new MapPath(id, name, start, end, t, length, p1x, p1y, p2x, p2y, locked,speed);
            g_onemap.addElement(path);
        }

        CppSQLite3Table table_bkg = g_db.getTable("select id,name,data,data_len,x,y,width,height,filename from agv_bkg;");
        if (table_bkg.numRows() > 0 && table_bkg.numFields() != 9)return false;
        for (int row = 0; row < table_bkg.numRows(); row++)
        {
            table_bkg.setRow(row);

            int id = atoi(table_bkg.fieldValue(0));
            std::string name = std::string(table_bkg.fieldValue(1));

            CppSQLite3Binary blob;
            blob.setEncoded((unsigned char*)table_bkg.fieldValue(2));
            unsigned char *data = new unsigned char[blob.getBinaryLength()];
            memcpy(data, blob.getBinary(), blob.getBinaryLength());
            int data_len = atoi(table_bkg.fieldValue(3));
            int x = atoi(table_bkg.fieldValue(4));
            int y = atoi(table_bkg.fieldValue(5));
            int width = atoi(table_bkg.fieldValue(6));
            int height = atoi(table_bkg.fieldValue(7));
            std::string filename = std::string(table_bkg.fieldValue(8));
            MapBackground *bkg = new MapBackground(id, name, (char *)data, data_len, width, height, filename);
            bkg->setX(x);
            bkg->setY(y);
            g_onemap.addElement(bkg);
        }
        int max_id = 0;
        auto ps = g_onemap.getAllElement();
        for (auto p : ps) {
            if (p->getId() > max_id)max_id = p->getId();
        }
        g_onemap.setMaxId(max_id);
        getReverseLines();
        getAdj();
        check();
    }
    catch (CppSQLite3Exception &e) {
       combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
        return false;
    }
    catch (std::exception e) {
       combined_logger->error("sqlerr code:{0}", e.what());
        return false;
    }
    return true;
}


//获取最优路径
std::vector<int> MapManager::getBestPath(int agv, int lastStation, int startStation, int endStation, int &distance, bool changeDirect)
{
    distance = DISTANCE_INFINITY;
    if (mapModifying) return std::vector<int>();
    int disA = DISTANCE_INFINITY;
    int disB = DISTANCE_INFINITY;

    std::vector<int> a = getPath(agv, lastStation, startStation, endStation, disA, false);
    std::vector<int> b;
    if (changeDirect) {
        b = getPath(agv, startStation, lastStation, endStation, disB, true);
        if (disA != DISTANCE_INFINITY && disB != DISTANCE_INFINITY) {
            distance = disA < disB ? disA : disB;
            if (disA < disB)return a;
            return b;
        }
    }
    if (disA != DISTANCE_INFINITY) {
        distance = disA;
        return a;
    }
    distance = disB;
    return b;
}

bool MapManager::pathPassable(MapPath *line, int agvId, std::vector<int> passable_uturnPoints) {

    auto pend = getPointById(line->getEnd());
    if(pend == nullptr)return false;

    if(std::find(passable_uturnPoints.begin(),passable_uturnPoints.end(), line->getStart()) == passable_uturnPoints.end() &&
            std::find(passable_uturnPoints.begin(), passable_uturnPoints.end(), line->getEnd()) == passable_uturnPoints.end())
    {
        //忽略起点和终点直连装卸货点及不可调头的判断
        if(pend->getPointType() == MapPoint::Map_Point_Type_CHARGE ||
                pend->getPointType() == MapPoint::Map_Point_Type_LOAD ||
                pend->getPointType() == MapPoint::Map_Point_Type_UNLOAD ||
                pend->getPointType() == MapPoint::Map_Point_Type_NO_UTURN ||
                pend->getPointType() == MapPoint::Map_Point_Type_LOAD_UNLOAD)return false;
    }

    //判断反向线路占用
    auto reverses = m_reverseLines[line->getId()];
    for(auto reverse:reverses){
        if (reverse > 0) {
            if (line_occuagvs[reverse].size() > 1 || (line_occuagvs[reverse].size() == 1 && *(line_occuagvs[reverse].begin()) != agvId))
                return false;
        }
    }

    //判断终点占用
    if (station_occuagv[line->getEnd()] != 0 && station_occuagv[line->getEnd()] != agvId)
        return false;


//    //判断group占用
//    auto groups = g_onemap.getGroups(COMMON_GROUP);
//    UNIQUE_LCK(groupMtx);
//    for (auto group : groups) {
//        auto sps = group->getElements();
//        if (std::find(sps.begin(), sps.end(), line->getId()) != sps.end() ||
//                std::find(sps.begin(), sps.end(), line->getEnd()) != sps.end())
//        {
//            //该线路/线路终点属于这个block
//            //判断该block是否有agv以外的其他agv
//            if (group_occuagv.find(group->getId()) != group_occuagv.end() && group_occuagv[group->getId()].first != agvId)
//                return false;
//        }
//    }

    return true;
}

bool MapManager::pathPassable(MapPath *line, int agvId) {
    //TODO: to delete
    if (line_occuagvs[line->getId()].size() > 1 || (line_occuagvs[line->getId()].size() == 1 && *(line_occuagvs[line->getId()].begin()) != agvId))
        return false;

    auto pend = getPointById(line->getEnd());
    if(pend == nullptr)return false;

    if(pend->getPointType() == MapPoint::Map_Point_Type_CHARGE ||
            pend->getPointType() == MapPoint::Map_Point_Type_LOAD ||
            pend->getPointType() == MapPoint::Map_Point_Type_UNLOAD ||
            pend->getPointType() == MapPoint::Map_Point_Type_LOAD_UNLOAD||
            pend->getPointType() == MapPoint::Map_Point_Type_NO_UTURN)return false;


    //判断反向线路占用
    auto reverses = m_reverseLines[line->getId()];
    for(auto reverse:reverses){
        if (reverse > 0) {
            if (line_occuagvs[reverse].size() > 1 || (line_occuagvs[reverse].size() == 1 && *(line_occuagvs[reverse].begin()) != agvId))
                return false;
        }
    }

    //判断终点占用
    if (station_occuagv[line->getEnd()] != 0 && station_occuagv[line->getEnd()] != agvId)
        return false;
    return true;
}

//获取 距离 目标点位 最近的躲避点//不计算可行性
int MapManager::getNearestHaltStation(int agvId, int aimStation)
{
    int halt = -1;
    int minDistance = DISTANCE_INFINITY;
    auto ae = g_onemap.getAllElement();
    for(auto e:ae){
        //mapElement ee;
        if(e->getElementType() == mapElement::Map_Element_Type_Point){
            if((static_cast<MapPoint *>(e))->getPointType() == MapPoint::Map_Point_Type_HALT)
            {
                //这是个避让点
                //如果这个躲避点已经被占用？
                if (station_occuagv[e->getId()] != 0 && station_occuagv[e->getId()] != agvId)
                    continue;

                //计算躲避点到目标点的最近距离
                int distance = DISTANCE_INFINITY;
                getPath(e->getId(),aimStation,distance,false);
                if(distance<minDistance)
                {
                    minDistance = distance;
                    halt = e->getId();
                }
            }
        }
    }

    return halt;
}

std::vector<int> MapManager::getPath(int from, int to, int &distance, bool changeDirect)
{
    std::vector<int> result;

    //判断station是否正确
    if (from <= 0 || to <=0) return result;
    if(from == to)
    {
        distance = 0;
        return result;
    }

    auto startStationPtr = g_onemap.getElementById(from);
    auto endStationPtr = g_onemap.getElementById(to);

    if (startStationPtr == nullptr || endStationPtr == nullptr)return result;

    if (startStationPtr->getElementType() != mapElement::Map_Element_Type_Point
            || endStationPtr->getElementType() != mapElement::Map_Element_Type_Point)
        return result;

    std::multimap<int, int> Q;// distance -- lineid
    auto paths = g_onemap.getPaths();

    struct LineDijkInfo {
        int father = 0;
        int distance = DISTANCE_INFINITY;
        int color = AGV_LINE_COLOR_WHITE;
    };
    std::map<int, LineDijkInfo> lineDistanceColors;

    for (auto path : paths) {
        lineDistanceColors[path->getId()].father = 0;
        lineDistanceColors[path->getId()].distance = DISTANCE_INFINITY;
        lineDistanceColors[path->getId()].color = AGV_LINE_COLOR_WHITE;
    }

    for (auto line : paths) {
        if (line->getStart() == from) {
            lineDistanceColors[line->getId()].distance = line->getLength();
            lineDistanceColors[line->getId()].color = AGV_LINE_COLOR_GRAY;
            Q.insert(std::make_pair(lineDistanceColors[line->getId()].distance, line->getId()));
        }
    }

    while (Q.size() > 0) {
        auto front = Q.begin();
        int startLine = front->second;

        std::vector<int> adjs = m_adj[startLine];
        for (auto adj : adjs)
        {
            if (lineDistanceColors[adj].color == AGV_LINE_COLOR_BLACK)continue;

            mapElement *pp = g_onemap.getElementById(adj);
            if (pp->getElementType() != mapElement::Map_Element_Type_Path)continue;
            MapPath *path = static_cast<MapPath *>(pp);
            if (lineDistanceColors[adj].color == AGV_LINE_COLOR_WHITE) {
                lineDistanceColors[adj].distance = lineDistanceColors[startLine].distance + path->getLength();
                lineDistanceColors[adj].color = AGV_LINE_COLOR_GRAY;
                lineDistanceColors[adj].father = startLine;
                Q.insert(std::make_pair(lineDistanceColors[adj].distance, adj));
            }
            else if (lineDistanceColors[adj].color == AGV_LINE_COLOR_GRAY) {
                if (lineDistanceColors[adj].distance > lineDistanceColors[startLine].distance + path->getLength()) {
                    lineDistanceColors[adj].distance = lineDistanceColors[startLine].distance + path->getLength();
                    lineDistanceColors[adj].father = startLine;

                    //更新Q中的 adj

                    //删除旧的
                    for (auto iiitr = Q.begin(); iiitr != Q.end();) {
                        if (iiitr->second == adj) {
                            iiitr = Q.erase(iiitr);
                        }
                        else
                            iiitr++;
                    }
                    //加入新的
                    Q.insert(std::make_pair(lineDistanceColors[adj].distance, adj));
                }
            }
        }
        lineDistanceColors[startLine].color = AGV_LINE_COLOR_BLACK;
        //erase startLine
        for (auto itr = Q.begin(); itr != Q.end();) {
            if (itr->second == startLine) {
                itr = Q.erase(itr);
            }
            else
                itr++;
        }
    }

    int index = 0;
    int minDis = DISTANCE_INFINITY;

    for (auto ll : paths) {
        if (ll->getEnd() == to) {
            if (lineDistanceColors[ll->getId()].distance < minDis) {
                minDis = lineDistanceColors[ll->getId()].distance;
                index = ll->getId();
            }
        }
    }

    distance = minDis;

    while (true) {
        if (index == 0)break;
        result.push_back(index);
        index = lineDistanceColors[index].father;
    }
    std::reverse(result.begin(), result.end());

    return result;
}


std::vector<int> MapManager::getPath(int agv, int lastStation, int startStation, int endStation, int &distance, bool changeDirect)
{
    std::vector<int> result;
    distance = DISTANCE_INFINITY;

    auto paths = g_onemap.getPaths();

    //判断station是否正确
    if(lastStation <=0 && startStation <=0 )return result;
    if (endStation <= 0)return result;
    if (lastStation <= 0) lastStation = startStation;
    if (startStation <= 0) startStation = lastStation;

    auto lastStationPtr = g_onemap.getElementById(lastStation);
    auto startStationPtr = g_onemap.getElementById(startStation);
    auto endStationPtr = g_onemap.getElementById(endStation);

    if (lastStationPtr == nullptr || startStationPtr == nullptr || endStationPtr == nullptr)return result;


    if (lastStationPtr->getElementType() != mapElement::Map_Element_Type_Point
            || startStationPtr->getElementType() != mapElement::Map_Element_Type_Point
            || endStationPtr->getElementType() != mapElement::Map_Element_Type_Point)
        return result;


    //判断站点占用清空
    if (station_occuagv[startStation] != 0 && station_occuagv[startStation] != agv)return result;
    if (station_occuagv[endStation] != 0 && station_occuagv[endStation] != agv)return result;

    if (startStation == endStation) {
        distance = 0;
        if (changeDirect && lastStation != startStation) {
            for (auto path : paths) {
                if (path->getStart() == lastStation && path->getEnd() == startStation) {
                    result.push_back(path->getId());
                    distance = path->getLength();
                }
            }
        }
        return result;
    }

    std::multimap<int, int> Q;// distance -- lineid

    struct LineDijkInfo {
        int father = 0;
        int distance = DISTANCE_INFINITY;
        int color = AGV_LINE_COLOR_WHITE;
    };
    std::map<int, LineDijkInfo> lineDistanceColors;

    std::vector<int> passable_uturnPoints;
    passable_uturnPoints.push_back(startStation);
    passable_uturnPoints.push_back(endStation);

    //初始化，所有线路 距离为无限远、color为尚未标记
    for (auto path : paths) {
        lineDistanceColors[path->getId()].father = 0;
        lineDistanceColors[path->getId()].distance = DISTANCE_INFINITY;
        lineDistanceColors[path->getId()].color = AGV_LINE_COLOR_WHITE;

        if(path->getStart() == startStation)
        {
            auto pend = getPointById(path->getEnd());
            if(pend == nullptr)continue;
            if(MapPoint::Map_Point_Type_NO_UTURN == pend->getPointType() || MapPoint::Map_Point_Type_LOAD == pend->getPointType()
                    || MapPoint::Map_Point_Type_LOAD == pend->getPointType() || MapPoint::Map_Point_Type_LOAD_UNLOAD == pend->getPointType())
            {
                passable_uturnPoints.push_back(pend->getId());
            }
        }
        else if(path->getEnd() == endStation)
        {
            auto pstart = getPointById(path->getStart());
            if(pstart == nullptr)continue;
            if(MapPoint::Map_Point_Type_NO_UTURN == pstart->getPointType()|| MapPoint::Map_Point_Type_LOAD == pstart->getPointType()
                    || MapPoint::Map_Point_Type_LOAD == pstart->getPointType() || MapPoint::Map_Point_Type_LOAD_UNLOAD == pstart->getPointType())
            {
                passable_uturnPoints.push_back(pstart->getId());
            }
        }
    }


    ////增加一种通行的判定：
    ////如果AGV2 要从 C点 到达 D点，同事路过B点。
    ////同时AGV1 要从 A点 到达 B点。如果AGV1先到达B点，会导致AGV2 无法继续运行。
    ////判定终点的线路 是否占用
    ////endPoint是终点
    {
        for (auto templine : paths) {
            if (templine->getStart() == endStation) {
                if (line_occuagvs[templine->getId()].size() > 1 || (line_occuagvs[templine->getId()].size() == 1 && *(line_occuagvs[templine->getId()].begin()) != agv)) {
                    //TODO:该方式到达这个地方 不可行.该线路 置黑、
                    lineDistanceColors[templine->getId()].color = AGV_LINE_COLOR_BLACK;
                    lineDistanceColors[templine->getId()].distance = DISTANCE_INFINITY;
                }
            }
        }
    }

    if (lastStation == startStation) {
        for (auto line : paths) {
            if (line->getStart() == lastStation) {
                if (pathPassable(line, agv,passable_uturnPoints)) {
                    if (lineDistanceColors[line->getId()].color == AGV_LINE_COLOR_BLACK)continue;
                    lineDistanceColors[line->getId()].distance = line->getLength();
                    lineDistanceColors[line->getId()].color = AGV_LINE_COLOR_GRAY;
                    Q.insert(std::make_pair(lineDistanceColors[line->getId()].distance, line->getId()));
                }
            }
        }
    }
    else {
        for (auto line : paths) {
            if (line->getStart() == startStation) {
                //            if (line->getStart() == startStation && line->getEnd() != lastStation) {
                if (pathPassable(line, agv,passable_uturnPoints)) {
                    if (lineDistanceColors[line->getId()].color == AGV_LINE_COLOR_BLACK)continue;
                    lineDistanceColors[line->getId()].distance = line->getLength();
                    lineDistanceColors[line->getId()].color = AGV_LINE_COLOR_GRAY;
                    Q.insert(std::make_pair(lineDistanceColors[line->getId()].distance, line->getId()));
                }
            }
        }
    }

    while (Q.size() > 0) {
        auto front = Q.begin();
        int startLine = front->second;

        std::vector<int> adjs = m_adj[startLine];
        for (auto adj : adjs)
        {
            if (lineDistanceColors[adj].color == AGV_LINE_COLOR_BLACK)continue;

            MapPath *path = g_onemap.getPathById(adj);
            if (path == nullptr)continue;
            if (!pathPassable(path, agv,passable_uturnPoints))continue;
            if (lineDistanceColors[adj].color == AGV_LINE_COLOR_WHITE) {
                lineDistanceColors[adj].distance = lineDistanceColors[startLine].distance + path->getLength();
                lineDistanceColors[adj].color = AGV_LINE_COLOR_GRAY;
                lineDistanceColors[adj].father = startLine;
                Q.insert(std::make_pair(lineDistanceColors[adj].distance, adj));
            }
            else if (lineDistanceColors[adj].color == AGV_LINE_COLOR_GRAY) {
                if (lineDistanceColors[adj].distance > lineDistanceColors[startLine].distance + path->getLength()) {
                    //更新father和距离
                    lineDistanceColors[adj].distance = lineDistanceColors[startLine].distance + path->getLength();
                    lineDistanceColors[adj].father = startLine;

                    //更新Q中的 adj

                    //删除旧的
                    for (auto iiitr = Q.begin(); iiitr != Q.end();) {
                        if (iiitr->second == adj) {
                            iiitr = Q.erase(iiitr);
                        }
                        else
                            iiitr++;
                    }
                    //加入新的
                    Q.insert(std::make_pair(lineDistanceColors[adj].distance, adj));
                }
            }
        }
        lineDistanceColors[startLine].color = AGV_LINE_COLOR_BLACK;
        //erase startLine
        for (auto itr = Q.begin(); itr != Q.end();) {
            if (itr->second == startLine) {
                itr = Q.erase(itr);
            }
            else
                itr++;
        }
    }

    int index = 0;
    int minDis = DISTANCE_INFINITY;

    for (auto ll : paths) {
        if (ll->getEnd() == endStation) {
            if (lineDistanceColors[ll->getId()].distance < minDis) {
                minDis = lineDistanceColors[ll->getId()].distance;
                index = ll->getId();
            }
        }
    }

    distance = minDis;

    while (true) {
        if (index == 0)break;
        result.push_back(index);
        index = lineDistanceColors[index].father;
    }
    std::reverse(result.begin(), result.end());

    if (result.size() > 0 && lastStation != startStation) {
        if (!changeDirect) {
            int  agv_line = *(result.begin());
            mapElement *sp = g_onemap.getElementById(agv_line);
            if (sp->getElementType() == mapElement::Map_Element_Type_Path) {
                MapPath *path = static_cast<MapPath *>(sp);
                if (path->getStart() == lastStation && path->getEnd() == startStation) {
                    result.erase(result.begin());
                }
            }
        }
    }

    return result;
}


void MapManager::getReverseLines()
{
    std::list<MapPath *> paths = g_onemap.getPaths();

    //TODO:对于临近的 a ->  b 和  b' -> a'进行反向判断
    for (auto a : paths) {
        for (auto b : paths) {
            if (a == b)continue;

            int aEndId = a->getEnd();
            int aStartId = a->getStart();
            int bStartId = b->getStart();
            int bEndId = b->getEnd();
            mapElement *aEnd =  getMapElementById(aEndId);
            mapElement *bStart =  getMapElementById(bStartId);
            mapElement *bEnd =  getMapElementById(bEndId);
            mapElement *aStart =  getMapElementById(aStartId);

            if(aEnd == nullptr || aEnd->getElementType() != mapElement::Map_Element_Type_Point
                    ||bEnd == nullptr || bEnd->getElementType() != mapElement::Map_Element_Type_Point
                    ||bStart == nullptr || bStart->getElementType() != mapElement::Map_Element_Type_Point
                    ||aStart == nullptr || aStart->getElementType() != mapElement::Map_Element_Type_Point)continue;

            MapPoint *pAEnd = static_cast<MapPoint *>(aEnd);
            MapPoint *pBStart = static_cast<MapPoint *>(bStart);
            MapPoint *pBEnd = static_cast<MapPoint *>(bEnd);
            MapPoint *pAStart = static_cast<MapPoint *>(aStart);

            auto lists = m_reverseLines[a->getId()];

            if (a->getEnd() == b->getStart() && a->getStart() == b->getEnd()) {
                lists.push_back(b->getId());
                m_reverseLines[a->getId()] = lists;
            }else if(pAEnd->getRealX() == pBStart->getRealX() && pAEnd->getRealY() == pBStart->getRealY()
                     && pBEnd->getRealX() == pAStart->getRealX() && pBEnd->getRealY() == pAStart->getRealY()){
                lists.push_back(b->getId());
                m_reverseLines[a->getId()] = lists;
            }
        }
    }
    return ;
}

//check if exists all element
void MapManager::check()
{
    //check lines!
    bool changed = false;
    auto paths = g_onemap.getPaths();
    for(auto path:paths){
        auto start = path->getStart();
        auto end = path->getEnd();
        if(g_onemap.getPointById(start) == nullptr
                ||g_onemap.getPointById(end) == nullptr){
            g_onemap.removeElementById(path->getId());
            changed = true;
        }
    }
    if(changed){
        save();
        changed = false;
    }

}

void MapManager::getAdj()
{
    std::list<MapPath *> paths = g_onemap.getPaths();

    for (auto a : paths) {
        for (auto b : paths) {
            if (a == b)continue;
            if (a->getEnd() == b->getStart() && a->getStart() != b->getEnd()) {
                m_adj[a->getId()].push_back(b->getId());
            }
        }
    }

    return ;
}

void MapManager::clear()
{
    line_occuagvs.clear();
    station_occuagv.clear();
  //  BlockManager::getInstance()->clear();
    m_reverseLines.clear();
    m_adj.clear();
    g_onemap.clear();
}


std::list<int> MapManager::getOccuElement(int agvId)
{
    std::list<int> l;
    for (auto s : station_occuagv) {
        if (s.second == agvId) {
            l.push_back(s.first);
        }
    }

    for (auto s : line_occuagvs) {
        if(std::find(s.second.begin(),s.second.end(),agvId)!=s.second.end())
        {
            l.push_back(s.first);
        }
    }
    return l;
}



void MapManager::interSetMap(SessionPtr conn, const Json::Value &request)
{
    Json::Value response;
    response["type"] = MSG_TYPE_RESPONSE;
    response["todo"] = request["todo"];
    response["queuenumber"] = request["queuenumber"];
    response["result"] = RETURN_MSG_RESULT_SUCCESS;

    if (TaskManager::getInstance()->hasTaskDoing())
    {
        response["result"] = RETURN_MSG_RESULT_FAIL;
        response["error_code"] = RETURN_MSG_ERROR_CODE_TASKING;
    }
    else {
        UserLogManager::getInstance()->push(conn->getUserName() + " set map");

        mapModifying = true;
        clear();
        try {
            g_db.execDML("delete from agv_station;");
            g_db.execDML("delete from agv_line;");
            g_db.execDML("delete from agv_bkg");
            g_db.execDML("delete from agv_block");
            g_db.execDML("delete from agv_group");

            //TODO:
            //1.解析站点
            for (int i = 0; i < request["points"].size(); ++i)
            {
                Json::Value station = request["points"][i];
                int id = station["id"].asInt();
                std::string name = station["name"].asString();
                int station_type = station["type"].asInt();
                int x = station["x"].asInt();
                int y = station["y"].asInt();
                int realX = station["realX"].asInt();
                int realY = station["realY"].asInt();
                int realA = station["realA"].asInt();
                int labelXoffset = station["labelXoffset"].asInt();
                int labelYoffset = station["labelYoffset"].asInt();
                bool mapchange = station["mapchange"].asBool();
                bool locked = station["locked"].asBool();
                std::string ip = station["ip"].asString();
                int port = station["port"].asInt();
                int agvType = station["agvType"].asInt();
                std::string lineId = station["lineId"].asString();
                MapPoint::Map_Point_Type t = static_cast<MapPoint::Map_Point_Type>(station_type);
                MapPoint *p = new MapPoint(id, name, t, x, y, realX, realY, realA, labelXoffset, labelYoffset, mapchange, locked,ip,port,agvType,lineId);
                g_onemap.addElement(p);
            }

            //2.解析线路
            for (int i = 0; i < request["paths"].size(); ++i)
            {
                Json::Value line = request["paths"][i];
                int id = line["id"].asInt();
                std::string name = line["name"].asString();
                int type = line["type"].asInt();
                int start = line["start"].asInt();
                int end = line["end"].asInt();
                int p1x = line["p1x"].asInt();
                int p1y = line["p1y"].asInt();
                int p2x = line["p2x"].asInt();
                int p2y = line["p2y"].asInt();
                int length = line["length"].asInt();
                bool locked = line["locked"].asBool();
                double speed = line["speed"].asDouble();

                MapPath::Map_Path_Type t = static_cast<MapPath::Map_Path_Type>(type);
                MapPath *p = new MapPath(id, name, start, end, t, length, p1x, p1y, p2x, p2y, locked,speed);
                g_onemap.addElement(p);
            }

            //4.解析背景图片
            for (int i = 0; i < request["bkgs"].size(); ++i)
            {
                Json::Value bkg = request["bkgs"][i];
                int id = bkg["id"].asInt();
                std::string name = bkg["name"].asString();
                std::string database64 = bkg["data"].asString();
                //combined_logger->debug("database64 length={0},\n left10={1},\n right10={2}", database64.length(), database64.substr(0, 10).c_str(), database64.substr(database64.length() - 10).c_str());

                int lenlen = Base64decode_len(database64.c_str());
                char *data = new char[lenlen];
                memset(data, 0, lenlen);
                lenlen = Base64decode(data, database64.c_str());

                ////TODO:输出16进制
                //for (int i = 0; i < 10; ++i) {
                //	printf(" %02X", data[i] & 0xFF);
                //}
                //printf("\n");

                //for (int i = 0; i < 10; ++i) {
                //	printf(" %02X", data[lenlen-10+i] & 0xFF);
                //}
                //printf("\n");


                int imgdatalen = bkg["data_len"].asInt();
                int width = bkg["width"].asInt();
                int height = bkg["height"].asInt();
                int x = bkg["x"].asInt();
                int y = bkg["y"].asInt();
                std::string filename = bkg["filename"].asString();
                MapBackground *p = new MapBackground(id, name, data, lenlen, width, height, filename);
                p->setX(x);
                p->setY(y);
                g_onemap.addElement(p);
            }


            int max_id = request["maxId"].asInt();
            g_onemap.setMaxId(max_id);


            //构建反向线路和adj
            getReverseLines();
            getAdj();

            if (!save()) {
                response["result"] = RETURN_MSG_RESULT_FAIL;
                response["error_code"] = RETURN_MSG_ERROR_CODE_SAVE_SQL_FAIL;
                clear();
            }
        }
        catch (CppSQLite3Exception e) {
            response["result"] = RETURN_MSG_RESULT_FAIL;
            response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
            std::stringstream ss;
            ss << "code:" << e.errorCode() << " msg:" << e.errorMessage();
            response["error_info"] = ss.str();
           //combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
        }
        catch (std::exception e) {
            response["result"] = RETURN_MSG_RESULT_FAIL;
            response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
            std::stringstream ss;
            ss << "info:" << e.what();
            response["error_info"] = ss.str();
           //combined_logger->error("sqlerr code:{0}", e.what());
        }
    }

    mapModifying = false;

    conn->send(response);
}

void MapManager::interGetMap(SessionPtr conn, const Json::Value &request)
{
    Json::Value response;
    response["type"] = MSG_TYPE_RESPONSE;
    response["todo"] = request["todo"];
    response["queuenumber"] = request["queuenumber"];
    response["result"] = RETURN_MSG_RESULT_SUCCESS;

    if (mapModifying) {
        response["result"] = RETURN_MSG_RESULT_FAIL;
        response["error_code"] = RETURN_MSG_ERROR_CODE_CTREATING;
    }
    else {
        UserLogManager::getInstance()->push(conn->getUserName() + " get map");

        std::list<mapElement *> allElement = g_onemap.getAllElement();

        Json::Value v_points;
        Json::Value v_paths;
        Json::Value v_floors;
        Json::Value v_bkgs;
//        Json::Value v_blocks;
//        Json::Value v_groups;
        for (auto Element : allElement)
        {
            if (Element->getElementType() == mapElement::Map_Element_Type_Point) {
                MapPoint *p = static_cast<MapPoint *>(Element);
                Json::Value pv;
                pv["id"] = p->getId();
                pv["name"] = p->getName();
                pv["type"] = p->getPointType();
                pv["x"] = p->getX();
                pv["y"] = p->getY();
                pv["realX"] = p->getRealX();
                pv["realY"] = p->getRealY();
                pv["realA"] = p->getRealA();
                pv["labelXoffset"] = p->getLabelXoffset();
                pv["labelYoffset"] = p->getLabelYoffset();
                pv["mapchange"] = p->getMapChange();
                pv["locked"] = p->getLocked();
                pv["ip"] = p->getIp();
                pv["port"] = p->getPort();
                pv["agvType"] = p->getAgvType();
                pv["lineId"] = p->getLineId();
                v_points.append(pv);
            }
            else if (Element->getElementType() == mapElement::Map_Element_Type_Path) {
                MapPath *p = static_cast<MapPath *>(Element);
                Json::Value pv;
                pv["id"] = p->getId();
                pv["name"] = p->getName();
                pv["type"] = p->getPathType();
                pv["start"] = p->getStart();
                pv["end"] = p->getEnd();
                pv["p1x"] = p->getP1x();
                pv["p1y"] = p->getP1y();
                pv["p2x"] = p->getP2x();
                pv["p2y"] = p->getP2y();
                pv["length"] = p->getLength();
                pv["locked"] = p->getLocked();
                pv["speed"] = p->getSpeed();
                v_paths.append(pv);
            }
            else if (Element->getElementType() == mapElement::Map_Element_Type_Background) {
                MapBackground *p = static_cast<MapBackground *>(Element);
                Json::Value pv;
                pv["id"] = p->getId();
                pv["name"] = p->getName();
                int lenlen = Base64encode_len(p->getImgDataLen());
                char *ss = new char[lenlen];
                Base64encode(ss, p->getImgData(), p->getImgDataLen());
                pv["data"] = std::string(ss, lenlen);
                delete[] ss;
                pv["data_len"] = p->getImgDataLen();
                pv["width"] = p->getWidth();
                pv["height"] = p->getHeight();
                pv["x"] = p->getX();
                pv["y"] = p->getY();
                pv["filename"] = p->getFileName();
                v_bkgs.append(pv);
            }

        }

        if (v_points.size() > 0)
            response["points"] = v_points;
        if (v_paths.size() > 0)
            response["paths"] = v_paths;
        if (v_floors.size() > 0)
            response["floors"] = v_floors;
        if (v_bkgs.size() > 0)
            response["bkgs"] = v_bkgs;
//        if (v_blocks.size() > 0)
//            response["blocks"] = v_blocks;
//        if (v_groups.size() > 0)
//            response["groups"] = v_groups;
        response["maxId"] = g_onemap.getMaxId();
    }
    conn->send(response);
}

void MapManager::interTrafficControlStation(SessionPtr conn, const Json::Value &request)
{
    Json::Value response;
    response["type"] = MSG_TYPE_RESPONSE;
    response["todo"] = request["todo"];
    response["queuenumber"] = request["queuenumber"];
    response["result"] = RETURN_MSG_RESULT_SUCCESS;

    if (mapModifying) {
        response["result"] = RETURN_MSG_RESULT_FAIL;
        response["error_code"] = RETURN_MSG_ERROR_CODE_CTREATING;
    }
    else {
        int id = request["id"].asInt();
        mapElement *Element = g_onemap.getElementById(id);
        if (Element == nullptr) {
            response["result"] = RETURN_MSG_RESULT_FAIL;
            response["error_code"] = RETURN_MSG_ERROR_CODE_UNFINDED;
        }
        else {
            if (Element->getElementType() == mapElement::Map_Element_Type_Point) {
                try {
                    CppSQLite3Buffer bufSQL;
                    MapPoint *station = static_cast<MapPoint *>(Element);
                    bufSQL.format("update agv_station set locked = 1 where id = %d;", station->getId());
                    g_db.execDML(bufSQL);
                    station->setLocked(true);
                }
                catch (CppSQLite3Exception e) {
                    response["result"] = RETURN_MSG_RESULT_FAIL;
                    response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
                    std::stringstream ss;
                    ss << "code:" << e.errorCode() << " msg:" << e.errorMessage();
                    response["error_info"] = ss.str();
                   //combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
                }
                catch (std::exception e) {
                    response["result"] = RETURN_MSG_RESULT_FAIL;
                    response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
                    std::stringstream ss;
                    ss << "info:" << e.what();
                    response["error_info"] = ss.str();
                   //combined_logger->error("sqlerr code:{0}", e.what());
                }
            }
            else {
                response["result"] = RETURN_MSG_RESULT_FAIL;
                response["error_code"] = RETURN_MSG_ERROR_CODE_UNFINDED;
                response["error_info"] = "not path or point";
            }
        }
    }
    conn->send(response);
}

void MapManager::interTrafficReleaseLine(SessionPtr conn, const Json::Value &request)
{
    Json::Value response;
    response["type"] = MSG_TYPE_RESPONSE;
    response["todo"] = request["todo"];
    response["queuenumber"] = request["queuenumber"];
    response["result"] = RETURN_MSG_RESULT_SUCCESS;

    if (mapModifying) {
        response["result"] = RETURN_MSG_RESULT_FAIL;
        response["error_code"] = RETURN_MSG_ERROR_CODE_CTREATING;
    }
    else {
        int id = request["id"].asInt();
        mapElement *Element = g_onemap.getElementById(id);
        if (Element == nullptr) {
            response["result"] = RETURN_MSG_RESULT_FAIL;
            response["error_code"] = RETURN_MSG_ERROR_CODE_UNFINDED;
        }
        else {
            if (Element->getElementType() == mapElement::Map_Element_Type_Path) {
                try {
                    CppSQLite3Buffer bufSQL;
                    MapPath *path = static_cast<MapPath *>(Element);
                    bufSQL.format("update agv_line set locked = 0 where id = %d;", path->getId());
                    g_db.execDML(bufSQL);
                    path->setLocked(false);
                }
                catch (CppSQLite3Exception e) {
                    response["result"] = RETURN_MSG_RESULT_FAIL;
                    response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
                    std::stringstream ss;
                    ss << "code:" << e.errorCode() << " msg:" << e.errorMessage();
                    response["error_info"] = ss.str();
                   //combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
                }
                catch (std::exception e) {
                    response["result"] = RETURN_MSG_RESULT_FAIL;
                    response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
                    std::stringstream ss;
                    ss << "info:" << e.what();
                    response["error_info"] = ss.str();
                   //combined_logger->error("sqlerr code:{0}", e.what());
                }
            }
            else {
                response["result"] = RETURN_MSG_RESULT_FAIL;
                response["error_code"] = RETURN_MSG_ERROR_CODE_UNFINDED;
                response["error_info"] = "not path or point";
            }
        }
    }
    conn->send(response);
}


void MapManager::interTrafficReleaseStation(SessionPtr conn, const Json::Value &request)
{
    Json::Value response;
    response["type"] = MSG_TYPE_RESPONSE;
    response["todo"] = request["todo"];
    response["queuenumber"] = request["queuenumber"];
    response["result"] = RETURN_MSG_RESULT_SUCCESS;

    if (mapModifying) {
        response["result"] = RETURN_MSG_RESULT_FAIL;
        response["error_code"] = RETURN_MSG_ERROR_CODE_CTREATING;
    }
    else {
        int id = request["id"].asInt();
        mapElement *Element = g_onemap.getElementById(id);
        if (Element == nullptr) {
            response["result"] = RETURN_MSG_RESULT_FAIL;
            response["error_code"] = RETURN_MSG_ERROR_CODE_UNFINDED;
        }
        else {
            if (Element->getElementType() == mapElement::Map_Element_Type_Point) {
                try {
                    CppSQLite3Buffer bufSQL;
                    MapPoint *station = static_cast<MapPoint *>(Element);
                    bufSQL.format("update agv_station set locked = 0 where id = %d;", station->getId());
                    g_db.execDML(bufSQL);
                    station->setLocked(false);
                }
                catch (CppSQLite3Exception e) {
                    response["result"] = RETURN_MSG_RESULT_FAIL;
                    response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
                    std::stringstream ss;
                    ss << "code:" << e.errorCode() << " msg:" << e.errorMessage();
                    response["error_info"] = ss.str();
                   //combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
                }
                catch (std::exception e) {
                    response["result"] = RETURN_MSG_RESULT_FAIL;
                    response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
                    std::stringstream ss;
                    ss << "info:" << e.what();
                    response["error_info"] = ss.str();
                   //combined_logger->error("sqlerr code:{0}", e.what());
                }
            }
            else {
                response["result"] = RETURN_MSG_RESULT_FAIL;
                response["error_code"] = RETURN_MSG_ERROR_CODE_UNFINDED;
                response["error_info"] = "not path or point";
            }
        }
    }
    conn->send(response);
}

void MapManager::interTrafficControlLine(SessionPtr conn, const Json::Value &request)
{
    Json::Value response;
    response["type"] = MSG_TYPE_RESPONSE;
    response["todo"] = request["todo"];
    response["queuenumber"] = request["queuenumber"];
    response["result"] = RETURN_MSG_RESULT_SUCCESS;

    if (mapModifying) {
        response["result"] = RETURN_MSG_RESULT_FAIL;
        response["error_code"] = RETURN_MSG_ERROR_CODE_CTREATING;
    }
    else {
        int id = request["id"].asInt();
        mapElement *Element = g_onemap.getElementById(id);
        if (Element == nullptr) {
            response["result"] = RETURN_MSG_RESULT_FAIL;
            response["error_code"] = RETURN_MSG_ERROR_CODE_UNFINDED;
        }
        else {
            if (Element->getElementType() == mapElement::Map_Element_Type_Path) {
                try {
                    CppSQLite3Buffer bufSQL;
                    MapPath *path = static_cast<MapPath *>(Element);
                    bufSQL.format("update agv_line set locked = 1 where id = %d;", path->getId());
                    g_db.execDML(bufSQL);
                    path->setLocked(true);
                }
                catch (CppSQLite3Exception e) {
                    response["result"] = RETURN_MSG_RESULT_FAIL;
                    response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
                    std::stringstream ss;
                    ss << "code:" << e.errorCode() << " msg:" << e.errorMessage();
                    response["error_info"] = ss.str();
                   //combined_logger->error("sqlerr code:{0} msg:{1}", e.errorCode(), e.errorMessage());
                }
                catch (std::exception e) {
                    response["result"] = RETURN_MSG_RESULT_FAIL;
                    response["error_code"] = RETURN_MSG_ERROR_CODE_QUERY_SQL_FAIL;
                    std::stringstream ss;
                    ss << "info:" << e.what();
                    response["error_info"] = ss.str();
                   //combined_logger->error("sqlerr code:{0}", e.what());
                }

            }
            else {
                response["result"] = RETURN_MSG_RESULT_FAIL;
                response["error_code"] = RETURN_MSG_ERROR_CODE_UNFINDED;
                response["error_info"] = "not path or point";
            }
        }
    }
    conn->send(response);
}

