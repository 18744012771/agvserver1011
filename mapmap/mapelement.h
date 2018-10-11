#ifndef MAPELEMENT_H
#define MAPELEMENT_H

#include <QObject>

class mapElement
{

public:
    explicit mapElement();
    enum Map_Element_Type
    {
        Map_Element_Type_Point = 0,//point
        Map_Element_Type_Path,//line
        //Map_Element_Type_Floor,
        Map_Element_Type_Background,
//        Map_Element_Type_Block,
//        Map_Element_Type_Group,
    };

   mapElement( int _id,std::string _name,Map_Element_Type _Element_type);

    mapElement(const mapElement &s) = delete;

    virtual ~mapElement();

    virtual mapElement *clone();

    bool operator ==(const mapElement &s){
        return id == s.id;
    }

   Map_Element_Type getElementType()
    {
        return element_type;
    }
    void setElementType(Map_Element_Type __type){element_type = __type;}

    int getId() const{return id;}
    void setId(int _id){id=_id;}

    std::string getName() const {return name;}
    void setName(std::string _name){name=_name;}
private:
    int id;
    std::string name;
    Map_Element_Type element_type;
signals:

public slots:
};

#endif // MAPELEMENT_H
