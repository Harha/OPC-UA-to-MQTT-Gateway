#ifndef OPCUACLIENT_H
#define OPCUACLIENT_H

#include <QThread>
#include <string>
#include <map>
#include <opc/ua/client/client.h>
#include <opc/ua/node.h>
#include <opc/ua/subscription.h>
#include "clientstates.h"

class MQTTClient;
class CouplerItem;

// --------------------------------------------------------
// Callback client class below
// --------------------------------------------------------
class OPCUASubClient : public OpcUa::SubscriptionHandler
{

public:
    OPCUASubClient(MQTTClient *cl = nullptr);

private:
    virtual void DataChange(uint32_t handle, const OpcUa::Node& node, const OpcUa::Variant& val, OpcUa::AttributeId attr) override;

    MQTTClient *m_mqttclient;

};

// --------------------------------------------------------
// OPC UA Client class below
// --------------------------------------------------------
class OPCUAClient : public QThread
{
    Q_OBJECT

public:
    OPCUAClient(MQTTClient *cl = nullptr, std::string ep = "opc.tcp://localhost:4841/");
    ~OPCUAClient();

    void createOpcUaMqttLink(CouplerItem *item, int period);
    void removeOpcUaMqttLink(CouplerItem *item);
    void requestEndpoints();
    void setInitEndpoint(std::string endpoint);
    void setTargetEndpoint(OpcUa::EndpointDescription endpoint);
    void setRunState(const CLIENT_STATE state);
    void setStatus(const CLIENT_STATUS status);
    std::string getInitEndpoint() const;
    std::vector<OpcUa::EndpointDescription> getEndpoints() const;
    OpcUa::EndpointDescription getTargetEndpoint() const;
    OpcUa::UaClient *getClient() const;
    OPCUASubClient *getSubClient() const;
    OpcUa::Node *getRootNode() const;
    OpcUa::Node *getObjectsNode() const;
    CLIENT_STATE getRunState() const;
    CLIENT_STATUS getStatus() const;
    static std::string securityLevelToString(int level);

private:
    MQTTClient *m_mqttclient;
    std::string m_initEndpoint;
    std::vector<OpcUa::EndpointDescription> m_endpoints;
    OpcUa::EndpointDescription m_targetEndpoint;
    OpcUa::UaClient *m_client;
    OPCUASubClient *m_subclient;
    std::map<std::string, std::unique_ptr<OpcUa::Subscription>> m_subs;
    OpcUa::Node *m_root;
    OpcUa::Node *m_objects;
    volatile CLIENT_STATE m_runstate;
    volatile CLIENT_STATUS m_status;

protected:
    void run() override;

};

#endif // OPCUACLIENT_H
