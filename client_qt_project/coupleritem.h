#ifndef COUPLERITEM_H
#define COUPLERITEM_H

#include <QObject>
#include <opc/ua/node.h>

class CouplerItem : public QObject
{
    Q_OBJECT

public:
    CouplerItem(QObject *parent = nullptr, OpcUa::Node opcuanode = OpcUa::Node());
    CouplerItem(const CouplerItem &obj);
    void setOpcUaNode(OpcUa::Node node);
    void setSubHandle(uint32_t handle);
    OpcUa::Node getOpcUaNode() const;
    uint32_t getSubHandle() const;
private:
    OpcUa::Node m_opcuanode;
    uint32_t m_subhandle;

};

Q_DECLARE_METATYPE(CouplerItem)
Q_DECLARE_METATYPE(CouplerItem *)

#endif // COUPLERITEM_H
