#include "mqttclient.h"
#include <QDebug>

// --------------------------------------------------------
// Callback functions below
// --------------------------------------------------------
void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
    MQTTClient *client = (MQTTClient *) obj;

    if (rc == 0) // Success
    {
        qDebug() << "MQTT: Successful connect!";

        client->setStatus(CONNECTED);
    }
    else if (rc == 1) // Refused (Unacceptable protocol version)
    {
        qDebug() << "MQTT: Connection refused (Unacceptable protocol version).";

        client->setStatus(ERROR);
    }
    else if (rc == 2) // Refused (Identifier rejected)
    {
        qDebug() << "MQTT: Connection refused (Identifier rejected).";

        client->setStatus(ERROR);
    }
    else if (rc == 3) // Refused (Broker unavailable)
    {
        qDebug() << "MQTT: Connection refused (Broker unavailable).";

        client->setStatus(ERROR);
    }
    else // Refused (Unknown)
    {
        qDebug() << "MQTT: Connection refused (Unknown).";

        client->setStatus(ERROR);
    }
}

void on_disconnect(struct mosquitto *mosq, void *obj, int rc)
{
    MQTTClient *client = (MQTTClient *) obj;

    if (rc == 0) // Disconnected via mosquitto_disconnect
    {
        qDebug() << "MQTT: Disconnected (mosquitto_disconnect).";

        client->setStatus(DISCONNECTED);
    }
    else // Unexpected disconnection
    {
        qDebug() << "MQTT: Disconnected (Unexpected).";

        client->setStatus(DISCONNECTED);
    }
}

void on_publish(struct mosquitto *mosq, void *obj, int rc)
{

}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{

}

void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{

}

void on_unsubscribe(struct mosquitto *mosq, void *obj, int mid)
{

}

void on_log(struct mosquitto *mosq, void *obj, int level, const char *str)
{
    //qDebug() << "MQTT: " << str;
}

// --------------------------------------------------------
// MQTTClient class below
// --------------------------------------------------------
MQTTClient::MQTTClient(std::string host, int port, int id, std::string topic) :
    m_client(NULL),
    m_host(host),
    m_port(port),
    m_id(id),
    m_topic(topic),
    m_runstate(NOTSTARTED),
    m_status(DISCONNECTED)
{
    mosqpp::lib_init();
    int major = 0, minor = 0, revision = 0;
    mosqpp::lib_version(&major, &minor, &revision);
    qDebug() << "MQTT: Mosquitto library, version " << major << minor << revision;
}

MQTTClient::~MQTTClient()
{
    destroy_client();
    mosqpp::lib_cleanup();
}

void MQTTClient::create_client()
{
    if (m_client == nullptr || m_client == NULL)
    {
        m_client = mosquitto_new(NULL, true, (void *) this); // pass pointer of this class as user data, to be used in callbacks
        mosquitto_connect_callback_set(m_client, on_connect);
        mosquitto_disconnect_callback_set(m_client, on_disconnect);
        mosquitto_publish_callback_set(m_client, on_publish);
        mosquitto_message_callback_set(m_client, on_message);
        mosquitto_subscribe_callback_set(m_client, on_subscribe);
        mosquitto_unsubscribe_callback_set(m_client, on_unsubscribe);
        mosquitto_log_callback_set(m_client, on_log);
    }
}

void MQTTClient::destroy_client()
{
    if (m_client != NULL)
    {
        if (m_status == CONNECTED)
            mosquitto_disconnect(m_client);

        mosquitto_destroy(m_client);
        m_client = NULL;
    }
}

void MQTTClient::publish_message(std::string subtopic, int payloadlen, const void *payload)
{
    mosquitto_publish(m_client, NULL, (m_topic + "/" + subtopic).c_str(), payloadlen, payload, 1, true);
}

void MQTTClient::run()
{
    if (m_runstate != NOTSTARTED && m_runstate != FINISHED)
        return;

    // Delete old instance of the client if it somehow still exists
    destroy_client();

    // Create client instance with random ID
    create_client();

    m_runstate = RUNNING;

    // Connect to target server, the error checking has to be done here like this because
    // connect callback doesn't work at this point.
    qDebug() << "MQTT: Connecting to" << m_host.c_str() << "...";
    int valueconnect = mosquitto_connect(m_client, m_host.c_str(), m_port, 60);

    if (valueconnect != MOSQ_ERR_SUCCESS)
    {
        qDebug() << "MQTT: Failed to connect to selected server.";
        m_runstate = NOTSTARTED;
        m_status = ERROR;
        return;
    }

    m_status = CONNECTED;

    if (m_status != CONNECTED)
    {
        m_runstate = NOTSTARTED;
        return;
    }

    // TODO: This could be set to be configurable via GUI
    int reconnAttempts = 0;
    int reconnMax = 100;

    while (m_runstate == RUNNING)
    {
        if (m_status == ERROR)
        {
            m_runstate = STOPPED;
        }
        else
        {
            if (reconnAttempts >= reconnMax)
            {
                m_status = DISCONNECTED;
                m_runstate = STOPPED;
            }

            if (m_status == DISCONNECTED && reconnAttempts < reconnMax)
            {
                qDebug() << "MQTT: Disconnected from server! Attempting to reconnect, attempts: " << reconnAttempts << "/" << reconnMax;

                int valuereconnect = mosquitto_reconnect(m_client);
                reconnAttempts++;

                /* <-- Not needed because we are relying completely on the callback functions now for state changes.
                if (valueconnect != MOSQ_ERR_SUCCESS)
                {
                    qDebug() << "MQTT: Failed to reconnect to the selected server.";
                }
                else
                {
                    qDebug() << "MQTT: Successfully reconnected to the selected server!";
                    reconnAttempts = 0;
                }
                */
            }

            mosquitto_loop(m_client, -1, 1);
        }
    }

    destroy_client();
    m_runstate = FINISHED;
    qDebug() << "MQTT: Disconnecting from server...";
}

void MQTTClient::setHost(std::string host)
{
    m_host = host;
}

void MQTTClient::setPort(int port)
{
    m_port = port;
}

void MQTTClient::setTopic(std::string topic)
{
    m_topic = topic;
}

void MQTTClient::setRunState(const CLIENT_STATE state)
{
    m_runstate = state;
}

void MQTTClient::setStatus(const CLIENT_STATUS status)
{
    m_status = status;
}

std::string MQTTClient::getHost() const
{
    return m_host;
}

int MQTTClient::getPort() const
{
    return m_port;
}

int MQTTClient::getId() const
{
    return m_id;
}

std::string MQTTClient::getTopic() const
{
    return m_topic;
}

CLIENT_STATE MQTTClient::getRunState() const
{
    return m_runstate;
}

CLIENT_STATUS MQTTClient::getStatus() const
{
    return m_status;
}
