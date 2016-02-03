#include "opcuaclient.h"
#include "mainwindow.h"
#include "mqttclient.h"
#include "coupleritem.h"
#include <QDebug>

// --------------------------------------------------------
// Callback client class below
// --------------------------------------------------------
OPCUASubClient::OPCUASubClient(MQTTClient *cli) : m_mqttclient(cli)
{

}

void OPCUASubClient::DataChange(uint32_t handle, const OpcUa::Node& node, const OpcUa::Variant& val, OpcUa::AttributeId attr)
{
    //qDebug() << "OPCUA: DataChange event, value of Node " << node.ToString().c_str() << " is now: "  << val.ToString().c_str();

    if (m_mqttclient->getStatus() == CONNECTED)
    {
        std::string strval = val.ToString();
        m_mqttclient->publish_message(std::to_string(node.GetId().GetNamespaceIndex()) + "/" + node.GetBrowseName().Name, strval.size(), (const void *) strval.c_str());
    }
}

// --------------------------------------------------------
// OPC UA Client class below
// --------------------------------------------------------
OPCUAClient::OPCUAClient(MQTTClient *cl, std::string ep) :
    m_mqttclient(cl),
    m_initEndpoint(ep),
    m_endpoints(std::vector<OpcUa::EndpointDescription>()),
    m_targetEndpoint(OpcUa::EndpointDescription()),
    m_client(new OpcUa::UaClient(false)),
    m_subclient(new OPCUASubClient(m_mqttclient)),
    m_subs(std::map<std::string, std::unique_ptr<OpcUa::Subscription>>()),
    m_root(nullptr),
    m_objects(nullptr),
    m_runstate(NOTSTARTED),
    m_status(DISCONNECTED)
{

}

OPCUAClient::~OPCUAClient()
{
    if (m_objects)
        delete m_objects;

    if (m_root)
        delete m_root;

    if (m_subclient)
        delete m_subclient;

    if (m_client)
    {
        if (m_runstate == RUNNING)
            m_runstate = STOPPED;

        if (m_status == CONNECTED)
            m_client->Disconnect();

        msleep(100);

        delete m_client;
    }
}

void OPCUAClient::createOpcUaMqttLink(CouplerItem *item, int period)
{
    OpcUa::Node node = item->getOpcUaNode();
    m_subs.insert(std::pair<std::string, std::unique_ptr<OpcUa::Subscription>>(node.ToString(), m_client->CreateSubscription((unsigned int) period, *m_subclient)));
    uint32_t handle = m_subs.at(node.ToString()).get()->SubscribeDataChange(node);
    item->setSubHandle(handle);
}

// TODO: Add getters for subscriptions and lock them with a mutex
void OPCUAClient::removeOpcUaMqttLink(CouplerItem *item)
{
    OpcUa::Subscription *sub = m_subs.at(item->getOpcUaNode().ToString()).get();
    sub->UnSubscribe(item->getSubHandle());
    m_subs.erase(item->getOpcUaNode().ToString());
    item->setSubHandle(0);
}

void OPCUAClient::requestEndpoints()
{
    if (m_runstate != NOTSTARTED && m_runstate != FINISHED)
        return;

    // Empty current endpoints
    m_endpoints.clear();

    try
    {
        qDebug() << "OPCUA: Getting endpoints from" << m_initEndpoint.c_str() << "...";
        m_endpoints = m_client->GetServerEndpoints(m_initEndpoint);
        m_client->Disconnect();
    }
    catch (const std::exception &exc)
    {
        qDebug() << exc.what();
        m_status = ERROR;
    }
    catch (...)
    {
        qDebug() << "Unknown error.";
        m_status = ERROR;
    }
}

void OPCUAClient::run()
{
    if (m_runstate != NOTSTARTED && m_runstate != FINISHED)
        return;

    // Prevent memory leak -> Delete old data
    if (m_root)
        delete m_root;
    if (m_objects)
        delete m_objects;

    // Delete old subscriptions
    if (!m_subs.empty())
        m_subs.clear();

    m_runstate = RUNNING;

    try
    {
        qDebug() << "OPCUA: Connecting to" << m_targetEndpoint.EndpointUrl.c_str() << "...";
        m_client->Connect(m_targetEndpoint);
        qDebug() << "OPCUA: Security policy: " << m_client->GetSecurityPolicy().c_str();
        m_status = CONNECTED;

        qDebug() << "OPCUA: Getting root & object nodes from the server...";
        m_root = new OpcUa::Node(m_client->GetRootNode());
        m_objects = new OpcUa::Node(m_client->GetObjectsNode());
        qDebug() << "OPCUA: Requested root node is" << m_root->ToString().c_str();
        qDebug() << "OPCUA: Requested objects node is" << m_objects->ToString().c_str();

        // The test below works, but emits errors. QT Doesn't like other threads
        // accessing the main UI thread widgets.
        // -------------------------------------
        // qDebug() << "OPCUA: Building treewidget nodes...";
        // m_mainwin->treeBuildOpcUa(m_root, nullptr, 0);
        // qDebug() << "OPCUA: Finished building the treewidget.";

        while (m_runstate == RUNNING)
        {
            if (m_status == ERROR)
                m_runstate = STOPPED;

            msleep(100); // Do something, currently nothing.
        }

        qDebug() << "OPCUA: Disconnecting from server...";
        m_client->Disconnect();
        m_status = DISCONNECTED;
    }
    catch (const std::exception &exc)
    {
        qDebug() << exc.what();
        m_status = ERROR;
        m_runstate = STOPPED;
    }
    catch (...)
    {
        qDebug() << "OPCUA: Unknown error.";
        m_status = ERROR;
        m_runstate = STOPPED;
    }

    m_runstate = FINISHED;
}

void OPCUAClient::setInitEndpoint(std::string endpoint)
{
    m_initEndpoint = endpoint;
}

void OPCUAClient::setTargetEndpoint(OpcUa::EndpointDescription endpoint)
{
    m_targetEndpoint = endpoint;
}

void OPCUAClient::setRunState(const CLIENT_STATE state)
{
    m_runstate = state;
}

void OPCUAClient::setStatus(const CLIENT_STATUS status)
{
    m_status = status;
}

std::string OPCUAClient::getInitEndpoint() const
{
    return m_initEndpoint;
}

std::vector<OpcUa::EndpointDescription> OPCUAClient::getEndpoints() const
{
    return m_endpoints;
}

OpcUa::EndpointDescription OPCUAClient::getTargetEndpoint() const
{
    return m_targetEndpoint;
}

OpcUa::UaClient *OPCUAClient::getClient() const
{
    return m_client;
}

OPCUASubClient *OPCUAClient::getSubClient() const
{
    return m_subclient;
}

OpcUa::Node *OPCUAClient::getRootNode() const
{
    return m_root;
}

OpcUa::Node *OPCUAClient::getObjectsNode() const
{
    return m_objects;
}

CLIENT_STATE OPCUAClient::getRunState() const
{
    return m_runstate;
}

CLIENT_STATUS OPCUAClient::getStatus() const
{
    return m_status;
}

std::string OPCUAClient::securityLevelToString(int level)
{
    if (level == 0)
        return "#None";
    else if (level == 1)
        return "#Basic128";
    else if (level == 2)
        return "#Basic256";
    else
        return "#Unknown";
}
