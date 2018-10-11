#ifndef MAPMANAGER_H
#define MAPMANAGER_H
#include <map>
#include <utility>
#include <algorithm>
#include <exception>
#include <atomic>
#include <vector>
#include "onemap.h"
#include <queue>
#include "agv/agv.h"
#include "util/common.h"
#include <boost/asio.hpp>
#include "network/tcpsession.h"
//#include "../Anting/station_pos.h"


class MapManager;
using MapManagerPtr = std::shared_ptr<MapManager>;
enum
{
    COMMON_GROUP = 1,//普通group
    HALT_GROUP = 2,
  //  ELEVATOR_GROUP = 3
};
//地图 由两部分助成： 点AgvStation 和 点之间的连线AgvLine
class MapManager : public boost::noncopyable, public std::enable_shared_from_this<MapManager>
{
public:

    static MapManagerPtr getInstance() {
        static MapManagerPtr m_inst = MapManagerPtr(new MapManager());
        return m_inst;
    }

    //载入地图
    bool load();
    //保存地图
    bool save();

    mapElement *getMapElementById(int id);

    mapElement *getMapElementByName(std::string name);

    MapPath *getMapPathByStartEnd(int start, int end);

    //一个Agv占领一个站点
    void addOccuStation(int station, AgvPtr occuAgv);

    //线路的反向占用//这辆车行驶方向和线路方向相反
    void addOccuLine(int line, AgvPtr occuAgv);

    //如果车辆占领该站点，释放
   void freeStation(int station, AgvPtr occuAgv);

    //如果车辆在线路的占领表中，释放出去
    void freeLine(int line, AgvPtr occuAgv);

    //获取最优路径
    std::vector<int> getBestPath(int agv, int lastStation, int startStation, int endStation, int &distance, bool changeDirect = CAN_CHANGE_DIRECTION);

    //获取 距离 目标点位 最近的躲避点
    int getNearestHaltStation(int agvId, int aimStation);


    MapPoint *getPointById(int id) { return g_onemap.getPointById(id); }

    MapPath *getPathById(int id) { return g_onemap.getPathById(id); }

    MapBackground *getBackgroundById(int id) { return g_onemap.getBackgroundById(id); }

    MapPath *getPathByStartEnd(int start,int end){return g_onemap.getPathByStartEnd(start,end);}   

    std::list<int> getOccuElement(int agvId);

    //用户接口
    void interSetMap(SessionPtr conn, const Json::Value &request);
    void interGetMap(SessionPtr conn, const Json::Value &request);

    void interTrafficControlStation(SessionPtr conn, const Json::Value &request);
    void interTrafficControlLine(SessionPtr conn, const Json::Value &request);
    void interTrafficReleaseStation(SessionPtr conn, const Json::Value &request);
    void interTrafficReleaseLine(SessionPtr conn, const Json::Value &request);


private:
    MapManager();

    //从配置或者数据库中载入地图
    bool loadFromDb();
    //清空所有地图信息
    void clear();

    OneMap g_onemap;//地图节点

    std::map<int, std::vector<int> > m_adj;// lineA -- lines{ from line A can reach which lines}
    std::map<int, std::vector<int> > m_reverseLines;

    std::map<int, std::vector<int> > line_occuagvs;//一条线路 及其上面的agv
    std::map<int, int> station_occuagv;//一个站点，及当前占用改站点的agv

    std::vector<int> getPath(int agv, int lastStation, int startStation, int endStation, int &distance, bool changeDirect);
    std::vector<int> getPath(int from, int to, int &distance, bool changeDirect = CAN_CHANGE_DIRECTION);
    void checkTable();

    void getReverseLines();
    void getAdj();
    void check();

    bool pathPassable(MapPath *line, int agvId);
    bool pathPassable(MapPath *line, int agvId, std::vector<int> passable_uturnPoints);


    std::atomic_bool mapModifying;

    std::map< std::pair<int, int>, std::queue<int> > m_chd_station;
};

#endif // MAPMANAGER_H
