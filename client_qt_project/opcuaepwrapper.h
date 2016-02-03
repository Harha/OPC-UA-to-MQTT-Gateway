#ifndef OPCUAEPWRAPPER_H
#define OPCUAEPWRAPPER_H

#include <QObject>
#include <opc/ua/client/client.h>

// --------------------------------------------------------
// OPCUAEpWrapper class, wraps an OpcUa::EndpointDescription object into a Q_OBJECT
// Stupid wrapper class to be stored as QVariant data in widgets
// --------------------------------------------------------
class OPCUAEpWrapper : public QObject
{
    Q_OBJECT

public:
    OPCUAEpWrapper(QObject *parent = nullptr, OpcUa::EndpointDescription ep = OpcUa::EndpointDescription());
    OPCUAEpWrapper(const OPCUAEpWrapper &obj);

    OpcUa::EndpointDescription getEndpoint() const;

private:
    OpcUa::EndpointDescription m_endpoint;

};

Q_DECLARE_METATYPE(OPCUAEpWrapper)
Q_DECLARE_METATYPE(OPCUAEpWrapper *)

#endif // OPCUAEPWRAPPER_H
