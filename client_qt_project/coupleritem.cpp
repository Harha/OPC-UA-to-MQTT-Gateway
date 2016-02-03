#include "coupleritem.h"

CouplerItem::CouplerItem(QObject *parent, OpcUa::Node opcuanode) :
    QObject(parent),
    m_opcuanode(opcuanode),
    m_subhandle(0)
{

}

CouplerItem::CouplerItem(const CouplerItem &obj) :
    QObject(obj.parent()),
    m_opcuanode(obj.getOpcUaNode())
{

}

void CouplerItem::setOpcUaNode(OpcUa::Node node)
{
    m_opcuanode = node;
}

void CouplerItem::setSubHandle(uint32_t handle)
{
    m_subhandle = handle;
}

OpcUa::Node CouplerItem::getOpcUaNode() const
{
    return m_opcuanode;
}

uint32_t CouplerItem::getSubHandle() const
{
    return m_subhandle;
}
