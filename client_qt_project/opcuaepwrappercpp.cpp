#include "opcuaepwrapper.h"

// --------------------------------------------------------
// OPCUAEpWrapper class, wraps an OpcUa::EndpointDescription object into a Q_OBJECT
// Stupid wrapper class to be stored as QVariant data in widgets
// --------------------------------------------------------
OPCUAEpWrapper::OPCUAEpWrapper(QObject *parent, OpcUa::EndpointDescription ep) :
    QObject(parent),
    m_endpoint(ep)
{

}

OPCUAEpWrapper::OPCUAEpWrapper(const OPCUAEpWrapper &obj) :
    QObject(obj.parent()),
    m_endpoint(obj.getEndpoint())
{

}

OpcUa::EndpointDescription OPCUAEpWrapper::getEndpoint() const
{
    return m_endpoint;
}
