#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QThread>
#include <string>
#include <mosquitto.h>
#include <cpp/mosquittopp.h>
#include "clientstates.h"

// --------------------------------------------------------
// Callback functions below
// --------------------------------------------------------
void on_connect(struct mosquitto *mosq, void *obj, int rc);
void on_disconnect(struct mosquitto *mosq, void *obj, int rc);
void on_publish(struct mosquitto *mosq, void *obj, int rc);
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
void on_unsubscribe(struct mosquitto *mosq, void *obj, int mid);
void on_log(struct mosquitto *mosq, void *obj, int level, const char *str);

// --------------------------------------------------------
// MQTTClient class below
//
// TODO: Add encryption support (SSL/TLS)
// --------------------------------------------------------
class MQTTClient : public QThread
{
    Q_OBJECT

public:
    MQTTClient(std::string host = "localhost", int port = 1883, int id = 0, std::string topic = "opcuamqtt");
    ~MQTTClient();

    void create_client();
    void destroy_client();
    void publish_message(std::string subtopic, int payloadlen, const void *payload);
    void setHost(std::string host);
    void setPort(int port);
    void setTopic(std::string topic);
    void setRunState(const CLIENT_STATE state);
    void setStatus(const CLIENT_STATUS status);
    std::string getHost() const;
    int getPort() const;
    int getId() const;
    std::string getTopic() const;
    CLIENT_STATE getRunState() const;
    CLIENT_STATUS getStatus() const;

private:
    mosquitto *m_client;
    std::string m_host;
    int m_port;
    int m_id;
    std::string m_topic;
    volatile CLIENT_STATE m_runstate;
    volatile CLIENT_STATUS m_status;

protected:
    void run() override;

};

#endif // MQTTCLIENT_H
