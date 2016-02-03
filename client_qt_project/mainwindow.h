#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <string>
#include "opcuaclient.h"
#include "mqttclient.h"

namespace Ui {
class MainWindow;
}

class AboutDialog;

// --------------------------------------------------------
// MainWindow class below
// --------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void createOpcUaMqttLink(QTreeWidgetItem *item);
    void removeOpcUaMqttLink(QTreeWidgetItem *item);
    QTreeWidgetItem *treeAddRoot(QTreeWidget *tree, OpcUa::Node *node);
    QTreeWidgetItem *treeAddChild(QTreeWidgetItem *parent, OpcUa::Node *node);
    void treeShowNodeInfo(QTreeWidgetItem *item);
    void treeSetNodeValue(QTreeWidgetItem *item, OpcUa::VariantType type);
    void treeAddNode(QTreeWidgetItem *item, int type);
    void treeBuildOpcUa(OpcUa::Node *node, QTreeWidgetItem *parent, int n);
    void setOpcUaStatus(CLIENT_STATUS status);
    void setMqttStatus(CLIENT_STATUS status);

public slots:
    void saveSettings();
    void loadSettings();
    void trigAboutMenu();
    void reqEndpointsOpcUaClient();
    void initOpcUaClient();
    void initMqttClient();
    void setMqttTopic();
    void treeUpdateItem(QTreeWidgetItem *item, int slot);
    void showOpcUaVarMenu(const QPoint &pos);

private:
    Ui::MainWindow *m_ui;
    AboutDialog *m_about;
    QString m_settingsFile;
    std::string m_opcua_addr;
    std::string m_mqtt_addr;
    OPCUAClient *m_opcua_client;
    MQTTClient *m_mqtt_client;

};

#endif // MAINWINDOW_H
