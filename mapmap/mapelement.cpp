#include "mapelement.h"

mapElement::mapElement()
{

}


mapElement::mapElement( int _id, std::string _name, Map_Element_Type _Element_type):

    element_type(_Element_type),
    id(_id),
    name(_name)
{

}

mapElement::~mapElement()
{

}

mapElement *mapElement::clone()
{
    mapElement *s = new mapElement(this->getId(),this->getName(),this->getElementType());
    return s;
}
