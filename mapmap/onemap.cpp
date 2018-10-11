#include "onemap.h"

OneMap::OneMap():
    max_id(0)
{

}

OneMap::~OneMap()
{
    for(auto f:all_element)delete f;
}

void OneMap::clear()
{
    for(auto f:all_element)delete f;
    all_element.clear();
    max_id = 0;
}

void OneMap::addElement(mapElement *Element)
{
    all_element.push_back(Element);
}

void OneMap::removeElement(mapElement *Element)
{
    all_element.remove(Element);
}

void OneMap::removeElementById(int id)
{
    for(auto itr = all_element.begin();itr!=all_element.end();){
        if((*itr)->getId() == id){
            itr = all_element.erase(itr);
        }else{
            itr++;
        }
    }
}

int OneMap::getNextId()
{
    return ++max_id;
}

//复制整个地图
OneMap *OneMap::clone()
{
    OneMap *onemap = new OneMap;
    onemap->setMaxId(max_id);
    for (auto e : all_element) {
        onemap->addElement(e->clone());
    }
    return onemap;
}

mapElement *OneMap::getElementById(int id)
{
    for (auto p : all_element) {
        if(p->getId() == id)return p;
    }
    return nullptr;
}

MapPoint *OneMap::getPointById(int id)
{
    for (auto p : all_element) {
        if (p->getId() == id && p->getElementType() == mapElement::Map_Element_Type_Point) {
            return static_cast<MapPoint *>(p);
        }
    }
    return nullptr;
}
MapPath *OneMap::getPathById(int id)
{
    for (auto p : all_element) {
        if (p->getId() == id && p->getElementType() == mapElement::Map_Element_Type_Path) {
            return static_cast<MapPath *>(p);
        }
    }
    return nullptr;
}
MapPath *OneMap::getPathByStartEnd(int start,int end)
{
    for (auto p : all_element) {
        if (p->getElementType() == mapElement::Map_Element_Type_Path) {
            auto pp = static_cast<MapPath *>(p);
            if(pp->getStart() == start && pp->getEnd() == end){
                return pp;
            }
        }
    }
    return nullptr;
}


MapBackground *OneMap::getBackgroundById(int id)
{
    for (auto p : all_element) {
        if (p->getId() == id && p->getElementType() == mapElement::Map_Element_Type_Background) {
            return static_cast<MapBackground *>(p);
        }
    }
    return nullptr;
}


std::vector<int> OneMap::getStations()
{
    std::vector<int> points;
    for(auto s:all_element)
    {
        if(s->getElementType() == mapElement::Map_Element_Type_Point) points.push_back(s->getId());
    }
    return points;
}



std::list<MapPath *> OneMap::getPaths()
{
    std::list<MapPath *> paths;
    for(auto s:all_element)
    {
        //在生成元素的时候会指定element类型
        if(s->getElementType() == mapElement::Map_Element_Type_Path)paths.push_back(static_cast<MapPath *>(s));
    }
    return paths;
}

