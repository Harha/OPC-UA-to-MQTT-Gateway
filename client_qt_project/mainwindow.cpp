#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "coupleritem.h"
#include "opcuaepwrapper.h"
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>

// --------------------------------------------------------
// MainWindow class below
// --------------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_about(nullptr),
    m_settingsFile(QApplication::applicationDirPath().left(1) + ":/settings.ini"),
    m_opcua_addr(std::string("")),
    m_mqtt_addr(std::string("")),
    m_opcua_client(nullptr),
    m_mqtt_client(nullptr)
{
    // Basic UI setup
    m_ui->setupUi(this);
    setWindowTitle("OPC UA -> MQTT Gateway client");
    setWindowIcon(QIcon(":/res/icon.ico"));

    qRegisterMetaType<CouplerItem>("CouplerItem");
    qRegisterMetaType<CouplerItem *>("CouplerItem *");
    qRegisterMetaType<OPCUAEpWrapper>("OPCUAEpWrapper");
    qRegisterMetaType<OPCUAEpWrapper *>("OPCUAEpWrapper *");

    m_ui->tv_opcua->setIndentation(16);
    m_ui->tv_opcua->setDragEnabled(false);
    m_ui->tv_opcua->setColumnCount(3);
    m_ui->tv_opcua->setHeaderLabels(QStringList() << "NdIdx" << "NsIdx" << "Name" << "Value");
    m_ui->tv_opcua->resizeColumnToContents(3);
    m_ui->tv_opcua->setContextMenuPolicy(Qt::CustomContextMenu);

    // Load GUI settings from ini file
    loadSettings();

    // Initialize opc ua / mqtt clients
    setMqttStatus(DISCONNECTED);
    m_mqtt_client = new MQTTClient("localhost", 1883, 0, "opcuamqtt");
    setOpcUaStatus(DISCONNECTED);
    m_opcua_client = new OPCUAClient(m_mqtt_client);

    // Signal -> Slot connections
    connect(m_ui->actionExit, SIGNAL(triggered(bool)),
            this, SLOT(close()));
    connect(m_ui->actionSave_settings, SIGNAL(triggered(bool)),
            this, SLOT(saveSettings()));
    connect(m_ui->actionLoad_settings, SIGNAL(triggered(bool)),
            this, SLOT(loadSettings()));
    connect(m_ui->actionAbout, SIGNAL(triggered(bool)),
            this, SLOT(trigAboutMenu()));
    connect(m_ui->pb_opcua_endp, SIGNAL(clicked(bool)),
            this, SLOT(reqEndpointsOpcUaClient()));
    connect(m_ui->pb_opcua_init, SIGNAL(clicked(bool)),
            this, SLOT(initOpcUaClient()));
    connect(m_ui->pb_mqtt_init, SIGNAL(clicked(bool)),
            this, SLOT(initMqttClient()));
    connect(m_ui->tv_opcua, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
            this, SLOT(treeUpdateItem(QTreeWidgetItem*, int)));
    connect(m_ui->tv_opcua, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showOpcUaVarMenu(QPoint)));
    connect(m_ui->le_mqtt_topic, SIGNAL(editingFinished()),
            this, SLOT(setMqttTopic()));

    // Make sure MainWindow is destroyed upon close
    setAttribute(Qt::WA_QuitOnClose);
}

MainWindow::~MainWindow()
{
    if (m_ui)
        delete m_ui;

    if (m_about)
        delete m_about;

    if (m_opcua_client)
    {
        if (m_opcua_client->getRunState() == RUNNING)
        {
            m_opcua_client->setRunState(STOPPED);

            while (m_opcua_client->getRunState() != FINISHED)
            {
                qDebug() << "MainThread: Waiting for OPCUA Client thread to finish.";

                QThread::sleep(1);
            }
        }

        delete m_opcua_client;
    }

    if (m_mqtt_client)
    {
        if (m_mqtt_client->getRunState() == RUNNING)
        {
            m_mqtt_client->setRunState(STOPPED);

            while (m_mqtt_client->getRunState() != FINISHED)
            {
                qDebug() << "MainThread: Waiting for MQTT Client thread to finish.";

                QThread::sleep(1);
            }
        }

        delete m_mqtt_client;
    }
}

void MainWindow::saveSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "OpcUaMqttGateway", "guisettings");

    QString s_opcua_addr = (m_ui->le_opcua_addr) ? m_ui->le_opcua_addr->text() : "opc.tcp://";
    QString s_mqtt_addr = (m_ui->le_mqtt_addr) ? m_ui->le_mqtt_addr->text() : "";
    QString s_mqtt_port = (m_ui->le_mqtt_port) ? m_ui->le_mqtt_port->text() : "1883";
    QString s_mqtt_topic = (m_ui->le_mqtt_topic) ? m_ui->le_mqtt_topic->text() : "opcuamqtt";

    settings.setValue("OpcUaInitAddr", s_opcua_addr);
    settings.setValue("MqttAddr", s_mqtt_addr);
    settings.setValue("MqttPort", s_mqtt_port);
    settings.setValue("MqttTopic", s_mqtt_topic);

    m_ui->le_opcua_addr->setText(s_opcua_addr);
    m_ui->le_mqtt_addr->setText(s_mqtt_addr);
    m_ui->le_mqtt_port->setText(s_mqtt_port);
    m_ui->le_mqtt_topic->setText(s_mqtt_topic);

    qDebug() << "Settings saved to" << settings.fileName();
}

void MainWindow::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "OpcUaMqttGateway", "guisettings");

    QString s_opcua_addr = settings.value("OpcUaInitAddr", "opc.tcp://").toString();
    QString s_mqtt_addr = settings.value("MqttAddr", "").toString();
    QString s_mqtt_port = settings.value("MqttPort", "1883").toString();
    QString s_mqtt_topic = settings.value("MqttTopic", "opcuamqtt").toString();

    m_ui->le_opcua_addr->setText(s_opcua_addr);
    m_ui->le_mqtt_addr->setText(s_mqtt_addr);
    m_ui->le_mqtt_port->setText(s_mqtt_port);
    m_ui->le_mqtt_topic->setText(s_mqtt_topic);

    qDebug() << "Settings loaded from" << settings.fileName();
}

void MainWindow::createOpcUaMqttLink(QTreeWidgetItem *item)
{
    // Return immediately if OpcUa/MQTT client is not running
    if (m_opcua_client->getStatus() != CONNECTED || m_mqtt_client->getStatus() != CONNECTED)
        return;

    // Get the node by casting the QVariant data in column 0 into CouplerItem*
    CouplerItem *item_coupler = item->data(0, Qt::UserRole).value<CouplerItem *>();
    OpcUa::Node node_opcua = item_coupler->getOpcUaNode();

    // If the data was invalid, return immediately with error.
    if (!item_coupler)
    {
        qDebug() << "Critical error! Couldn't convert tree node from QVariant to CouplerItem*";
        return;
    }

    // If the subscription exists, leave
    if (item_coupler->getSubHandle() != 0)
    {
        QMessageBox::warning(this, "Subscription already found", "There was an alive subscription found for this node. Link failed.");
        return;
    }

    // Add a subscription, later add option for user to set sub time + other stuff.
    try
    {
        m_opcua_client->createOpcUaMqttLink(item_coupler, 60);
    }
    catch (const std::exception &e)
    {
        qDebug() << e.what();

        // Also show error as box to user
        QMessageBox::warning(this, "Exception", QString(e.what()));
    }
}

void MainWindow::removeOpcUaMqttLink(QTreeWidgetItem *item)
{
    // Return immediately if OpcUa/MQTT client is not running
    if (m_opcua_client->getStatus() != CONNECTED || m_mqtt_client->getStatus() != CONNECTED)
        return;

    // Get the node by casting the QVariant data in column 0 into CouplerItem*
    CouplerItem *item_coupler = item->data(0, Qt::UserRole).value<CouplerItem *>();
    OpcUa::Node node_opcua = item_coupler->getOpcUaNode();

    // If the data was invalid, return immediately with error.
    if (!item_coupler)
    {
        qDebug() << "Critical error! Couldn't convert tree node from QVariant to CouplerItem*";
        return;
    }

    // If the subscription doesn't exist, leave
    if (item_coupler->getSubHandle() == 0)
    {
        QMessageBox::warning(this, "Subscription not found", "There was no subscription found for this node. Unlink failed.");
        return;
    }

    // Remove the subscription
    try
    {
        m_opcua_client->removeOpcUaMqttLink(item_coupler);
    }
    catch (const std::exception &e)
    {
        qDebug() << e.what();

        // Also show error as box to user
        QMessageBox::warning(this, "Exception", QString(e.what()));
    }
}

QTreeWidgetItem *MainWindow::treeAddRoot(QTreeWidget *tree, OpcUa::Node *node)
{
    // Create new item
    QTreeWidgetItem *item = new QTreeWidgetItem(tree);

    // Assign node as data to it
    QVariant data = QVariant::fromValue(new CouplerItem(this, OpcUa::Node(*node)));
    item->setData(0, Qt::UserRole, data);

    // Update item info
    treeUpdateItem(item, 0);

    // Add the item to the tree
    tree->addTopLevelItem(item);

    // Return result
    return item;
}

QTreeWidgetItem *MainWindow::treeAddChild(QTreeWidgetItem *parent, OpcUa::Node *node)
{
    // Create new item
    QTreeWidgetItem *item = new QTreeWidgetItem();

    // Assign node as data to it
    QVariant data = QVariant::fromValue(new CouplerItem(this, OpcUa::Node(*node)));
    item->setData(0, Qt::UserRole, data);

    // Update item info
    treeUpdateItem(item, 0);

    // Add the item to the tree
    parent->addChild(item);

    // Return result
    return item;
}

void MainWindow::treeShowNodeInfo(QTreeWidgetItem *item)
{
    // Return immediately if OpcUa client is not running
    if (m_opcua_client->getStatus() != CONNECTED)
        return;

    // Get the node by casting the QVariant data in column 0 into CouplerItem*
    CouplerItem *item_coupler = item->data(0, Qt::UserRole).value<CouplerItem *>();
    OpcUa::Node node_opcua = item_coupler->getOpcUaNode();

    // If the data was invalid, return immediately with error.
    if (!item_coupler)
    {
        qDebug() << "Critical error! Couldn't convert tree node from QVariant to CouplerItem*";
        return;
    }

    // Get node info as string
    std::string node_info;
    try
    {
        node_info.append("Object: " + node_opcua.ToString() + "\n");
        node_info.append("Name: " + node_opcua.GetBrowseName().Name + "\n");
        node_info.append("Type: " + node_opcua.GetDataType().ToString() + "\n");
        node_info.append("Value: " + node_opcua.GetValue().ToString() + "\n");
        std::stringstream ss1;
        ss1 << "Childs: " << node_opcua.GetChildren().size();
        node_info.append("Childs: " + ss1.str() + "\n");
        std::stringstream ss2;
        ss2 << "Variables: " << node_opcua.GetVariables().size();
        node_info.append("Variables: " + ss2.str() + "\n");
    }
    catch (const std::exception &e)
    {
        qDebug() << e.what();

        // Also show error as box to user
        QMessageBox::warning(this, "Exception", QString(e.what()));
    }

    // Show node info in messagebox to user
    QMessageBox box_info(this);
    box_info.setWindowTitle("Node info");
    box_info.setText(QString::fromStdString(node_info));
    box_info.exec();
}

void MainWindow::treeSetNodeValue(QTreeWidgetItem *item, OpcUa::VariantType type)
{
    // Return immediately if OpcUa client is not running
    if (m_opcua_client->getStatus() != CONNECTED)
        return;

    // Get the node by casting the QVariant data in column 0 into CouplerItem*
    CouplerItem *item_coupler = item->data(0, Qt::UserRole).value<CouplerItem *>();
    OpcUa::Node node_opcua = item_coupler->getOpcUaNode();

    // If the data was invalid, return immediately with error.
    if (!item_coupler)
    {
        qDebug() << "Critical error! Couldn't convert tree node from QVariant to CouplerItem*";
        return;
    }

    // Request user input
    bool dialog_ok;
    int value_int;
    float value_float;
    bool value_bool;
    std::string value_string;
    try
    {
        switch (type)
        {
        case OpcUa::VariantType::INT16:
        case OpcUa::VariantType::INT32:
        case OpcUa::VariantType::INT64:
            value_int = QInputDialog::getInt(this, "Set new int value", "Input a new integer value", 0, INT_MIN, INT_MAX, 1, &dialog_ok);
            if (dialog_ok)
            {
                node_opcua.SetValue(static_cast<int16_t>(value_int));
            }
            break;
        case OpcUa::VariantType::FLOAT:
            value_float = static_cast<float>(QInputDialog::getDouble(this, "Set new float value", "Input a new floating point value", 0, INT_MIN, INT_MAX, 2, &dialog_ok));
            if (dialog_ok)
            {
                node_opcua.SetValue(value_float);
            }
            break;
        case OpcUa::VariantType::BOOLEAN:
            value_bool = static_cast<bool>(QInputDialog::getInt(this, "Set new bool value", "Input a new boolean value", 0, 0, 1, 1, &dialog_ok));
            if (dialog_ok)
            {
                node_opcua.SetValue(value_bool);
            }
            break;
        case OpcUa::VariantType::STRING:
            value_string.append(QInputDialog::getText(this, "Set new string value", "Input a new string value", QLineEdit::Normal, "", &dialog_ok).toUtf8());
            if (dialog_ok)
            {
                node_opcua.SetValue(value_string);
            }
            break;
        default:
            qDebug() << "Set new value failed. Unsupported type.";
            break;
        }

        // Update tree item
        treeUpdateItem(item, 0);
    }
    catch (const std::exception &e)
    {
        qDebug() << e.what();

        // Also show error as box to user
        QMessageBox::warning(this, "Exception", QString(e.what()));
    }

    /*
    bool dialog_ok;
    QString str_value = QInputDialog::getText(this, "Set node value", "Input a new value", QLineEdit::Normal, "", &dialog_ok);

    // If user chose ok
    if (dialog_ok)
    {
        // Check if number or text
        bool flt_ok;
        float flt_value = str_value.toFloat(&flt_ok);

        // Set value, first float, if not then text
        if (flt_ok)
        {
            node_opcua.SetValue(OpcUa::Variant(flt_value));
        }
        else
        {
            node_opcua.SetValue(OpcUa::Variant(str_value.toStdString()));
        }

        // Update tree item
        treeUpdateItem(item, 0);
    }
    */
}

void MainWindow::treeAddNode(QTreeWidgetItem *item, int type)
{
    // Return immediately if OpcUa client is not running
    if (m_opcua_client->getStatus() != CONNECTED)
        return;

    // Get the node by casting the QVariant data in column 0 into CouplerItem*
    CouplerItem *item_coupler = item->data(0, Qt::UserRole).value<CouplerItem *>();
    OpcUa::Node node_opcua = item_coupler->getOpcUaNode();

    // If the data was invalid, return immediately with error.
    if (!item_coupler)
    {
        qDebug() << "Critical error! Couldn't convert tree node from QVariant to CouplerItem*";
        return;
    }

    bool dialog_ok;
    std::string value_string;
    OpcUa::Node new_node;
    try
    {
        switch (type)
        {
        case 0:
            value_string.append(QInputDialog::getText(this, "Add new folder node", "Input a new browsename", QLineEdit::Normal, "", &dialog_ok).toUtf8());
            if (dialog_ok)
            {
                new_node = node_opcua.AddFolder(node_opcua.GetId().GetNamespaceIndex(), value_string);
            }
            break;
        case 1:
            value_string.append(QInputDialog::getText(this, "Add new variable node", "Input a new browsename", QLineEdit::Normal, "", &dialog_ok).toUtf8());
            if (dialog_ok)
            {
                new_node = node_opcua.AddVariable(node_opcua.GetId().GetNamespaceIndex(), value_string, OpcUa::Variant(std::string("new variable")));
            }
            break;
        }

        // Add new item to tree
        treeAddChild(item, &new_node);
    }
    catch (const std::exception &e)
    {
        qDebug() << e.what();

        // Also show error as box to user
        QMessageBox::warning(this, "Exception", QString(e.what()));
    }

}

void MainWindow::treeBuildOpcUa(OpcUa::Node *node, QTreeWidgetItem *parent, int n)
{
    // Process QT Events while building on the main thread
    QApplication::processEvents();

    // Return immediately if OpcUa client has requested quit
    if (m_opcua_client->getRunState() != RUNNING)
        return;

    // Check if we are at root level, if so add node as root,
    // parent at this time will be the current *node
    // At first I used n to check at what level we are,
    // but since that caused incorrect folder structuring
    // I tried this and it works, but doesn't allow
    // more than a singular real root node
    if (!parent)
        parent = treeAddRoot(m_ui->tv_opcua, node);
    else
        parent = treeAddChild(parent, node);

    // Maximum recursion reached -> return
    if (n > 64)
        return;

    // Traverse childs, parent for all executed treeBuildOpcUa calls is *parent
    std::vector<OpcUa::Node> node_children = node->GetChildren();
    for (OpcUa::Node c : node_children)
    {
        treeBuildOpcUa(&c, parent, n++);
    }
}

void MainWindow::setOpcUaStatus(CLIENT_STATUS status)
{
    if (status == CONNECTED)
    {
        m_ui->lb_opcua_status->setText("Connected");
        m_ui->lb_opcua_status->setStyleSheet("QLabel { color: green }");
        m_ui->pb_opcua_init->setText("Disconnect");
    }
    else if (status == DISCONNECTED)
    {
        m_ui->lb_opcua_status->setText("Disconnected");
        m_ui->lb_opcua_status->setStyleSheet("QLabel { color: red }");
        m_ui->pb_opcua_init->setText("Connect");
    }
    else
    {
        m_ui->lb_opcua_status->setText("Error!");
        m_ui->lb_opcua_status->setStyleSheet("QLabel { color: blue }");
        m_ui->pb_opcua_init->setText("Reconnect");
    }
}

void MainWindow::setMqttStatus(CLIENT_STATUS status)
{
    if (status == CONNECTED)
    {
        m_ui->lb_mqtt_status->setText("Connected");
        m_ui->lb_mqtt_status->setStyleSheet("QLabel { color: green }");
        m_ui->pb_mqtt_init->setText("Disconnect");
    }
    else if (status == DISCONNECTED)
    {
        m_ui->lb_mqtt_status->setText("Disconnected");
        m_ui->lb_mqtt_status->setStyleSheet("QLabel { color: red }");
        m_ui->pb_mqtt_init->setText("Connect");
    }
    else
    {
        m_ui->lb_mqtt_status->setText("Error!");
        m_ui->lb_mqtt_status->setStyleSheet("QLabel { color: blue }");
        m_ui->pb_mqtt_init->setText("Reconnect");
    }
}

void MainWindow::trigAboutMenu()
{
    if (m_about)
        m_about->close();

    m_about = new AboutDialog(this);
    m_about->show();
}

void MainWindow::reqEndpointsOpcUaClient()
{
    if (m_opcua_client->getStatus() != CONNECTED)
    {
        // Clear old endpoints UI-wise
        m_ui->cb_opcua_endpoints->clear();

        // Request endpoints from target server
        m_opcua_client->setInitEndpoint(m_ui->le_opcua_addr->text().toStdString());
        m_opcua_client->requestEndpoints();

        for (OpcUa::EndpointDescription &ep : m_opcua_client->getEndpoints())
        {
            m_ui->cb_opcua_endpoints->addItem( // <-- Kinda hacky approach but whatever.
                                               QString::fromStdString(ep.EndpointUrl + OPCUAClient::securityLevelToString(ep.SecurityLevel)),
                                               QVariant::fromValue(new OPCUAEpWrapper(OPCUAEpWrapper(this, ep))));
        }
    }
}

void MainWindow::initOpcUaClient()
{
    if (m_opcua_client->getStatus() != CONNECTED)
    {
        // Connect to the selected endpoint
        OPCUAEpWrapper *selectedEndpoint = m_ui->cb_opcua_endpoints->currentData(Qt::UserRole).value<OPCUAEpWrapper *>();

        if (!selectedEndpoint)
        {
            QMessageBox::warning(this, "Error!", QString("Invalid endpoint data in dropdown list!"));
            return;
        }

        m_opcua_client->setTargetEndpoint(selectedEndpoint->getEndpoint());

        m_ui->tv_opcua->clear();
        m_opcua_client->start();
        QThread::msleep(1000);

        // Update status
        setOpcUaStatus(m_opcua_client->getStatus());

        if (m_opcua_client->getStatus() == CONNECTED)
        {
            // Update treeview nodes
            qDebug() << "OPCUA: Building treewidget nodes...";
            m_ui->tv_opcua->clear();

            if (m_ui->rb_opcua_root->isChecked())
            {
                treeBuildOpcUa(m_opcua_client->getRootNode(), nullptr, 0);
            }
            else
            {
                treeBuildOpcUa(m_opcua_client->getObjectsNode(), nullptr, 0);
            }

            qDebug() << "OPCUA: Finished building the treewidget.";
        }
        else
        {
            std::string error("Error connecting to the selected endpoint.\n(" + m_opcua_client->getInitEndpoint() +")");
            qDebug() << error.c_str();

            // Show the error message to the user
            QMessageBox::warning(this, "Connection error", QString::fromStdString(error));
        }
    }
    else
    {
        m_opcua_client->setRunState(STOPPED);

        while (m_opcua_client->getStatus() == CONNECTED)
        {
            QThread::msleep(100);
        }

        setOpcUaStatus(m_opcua_client->getStatus());
    }
}

void MainWindow::initMqttClient()
{
    if (m_mqtt_client->getStatus() != CONNECTED)
    {
        // Connect to the selected MQTT server
        m_mqtt_client->setHost(m_ui->le_mqtt_addr->text().toStdString());
        m_mqtt_client->setPort(m_ui->le_mqtt_port->text().toInt());
        m_mqtt_client->start();
        QThread::msleep(1000);

        // Update status
        setMqttStatus(m_mqtt_client->getStatus());

        if (m_mqtt_client->getStatus() == CONNECTED)
        {
            // Do whatever, atm nothing is needed.
        }
        else
        {
            std::string error("Error connecting to the selected server.\n(" + m_mqtt_client->getHost() + ")");

            // Show the error message to the user
            QMessageBox::warning(this, "Connection error", QString::fromStdString(error));
        }
    }
    else
    {
        m_mqtt_client->setRunState(STOPPED);

        while (m_mqtt_client->getStatus() == CONNECTED)
        {
            QThread::msleep(100);
        }

        setMqttStatus(m_mqtt_client->getStatus());
    }
}

void MainWindow::setMqttTopic()
{
    m_mqtt_client->setTopic(m_ui->le_mqtt_topic->text().toStdString());
}

void MainWindow::treeUpdateItem(QTreeWidgetItem *item, int slot)
{
    // Return immediately if OpcUa client is not running
    if (m_opcua_client->getStatus() != CONNECTED)
        return;

    // Get the node by casting the QVariant data in column 0 into CouplerItem*
    CouplerItem *item_coupler = item->data(0, Qt::UserRole).value<CouplerItem *>();
    OpcUa::Node node_opcua = item_coupler->getOpcUaNode();

    // If the data was invalid, return immediately with error.
    if (!item_coupler)
    {
        qDebug() << "Critical error! Couldn't convert tree node from QVariant to CouplerItem*";
        return;
    }

    // Get node info as strings
    QString node_id;
    QString node_ns;
    QString node_name;
    QString node_value;
    try
    {
        if (node_opcua.GetId().IsInteger())
        {
            node_id = QString::number(node_opcua.GetId().GetIntegerIdentifier());
        } else if(node_opcua.GetId().IsString())
        {
            node_id = QString::fromStdString(node_opcua.GetId().GetStringIdentifier());
        }
        else
        {
            qDebug() << "Unsupported nodeid encoding type! Only integer & std::string are supported.";
            node_id = "-1";
        }

        node_ns = QString::number(node_opcua.GetId().GetNamespaceIndex());
        node_name = QString::fromStdString(node_opcua.GetBrowseName().Name);
        node_value = QString::fromStdString(node_opcua.GetValue().ToString());
        if (node_value.startsWith("conversion"))
        {
            /* <-- Just testing... Beckhoff has many unsupported types.
            if (node_opcua.GetValue().Type() == OpcUa::VariantType::LOCALIZED_TEXT)
                qDebug() << "Found unsupported variant of type LOCALIZED_TEXT! name: " << node_name;
            else if (node_opcua.GetValue().Type() == OpcUa::VariantType::BOOLEAN)
                qDebug() << "Found unsupported variant of type BOOLEAN! name: " << node_name;
            else if (node_opcua.GetValue().Type() == OpcUa::VariantType::QUALIFIED_NAME)
                qDebug() << "Found unsupported variant of type QUALIFIED_NAME! name: " << node_name;
            else if (node_opcua.GetValue().Type() == OpcUa::VariantType::NODE_Id)
                qDebug() << "Found unsupported variant of type NODE_Id! name: " << node_name;
            else if (node_opcua.GetValue().Type() == OpcUa::VariantType::GUId)
                qDebug() << "Found unsupported variant of type GUId! name: " << node_name;
            else  if (node_opcua.GetValue().Type() == OpcUa::VariantType::STATUS_CODE)
                qDebug() << "Found unsupported variant of type STATUS_CODE! name: " << node_name;
            else  if (node_opcua.GetValue().Type() == OpcUa::VariantType::VARIANT)
                qDebug() << "Found unsupported variant of type VARIANT! name: " << node_name;
            else  if (node_opcua.GetValue().Type() == OpcUa::VariantType::DIAGNOSTIC_INFO)
                qDebug() << "Found unsupported variant of type DIAGNOSTIC_INFO! name: " << node_name;
            */
            node_value = "";
        }
    }
    catch (const std::exception &e)
    {
        qDebug() << e.what();

        node_id = "-1";
        node_ns = "-1";
        node_name = "null";
        node_value = "";
    }

    // Assign the info into current tree item
    item->setText(0, node_id);
    item->setText(1, node_ns);
    item->setText(2, node_name);

    if (!node_value.isEmpty())
    {
        item->setText(3, node_value);
        if (item_coupler->getSubHandle() == 0)
        {
            item->setBackgroundColor(3, QColor(192, 255, 64));
        }
        else
        {
            item->setBackgroundColor(3, QColor(64, 192, 255));
        }
    }

    // Inform user about the update
    m_ui->statusBar->showMessage("Updating (" + node_name + ") node status in the treeview.", 5000);
}

void MainWindow::showOpcUaVarMenu(const QPoint &pos)
{
    // Return if clients are offline
    if (m_opcua_client->getRunState() != RUNNING)
        return;

    // Get the tree and item
    QTreeWidget *tree = m_ui->tv_opcua;
    QTreeWidgetItem *item = tree->itemAt(pos);

    // Return if item does not exist
    if (!item)
        return;

    // Create action
    QAction *action1 = new QAction("Info", this);
    action1->setStatusTip("Shows more detailed information about the selected node.");

    QAction *action2_1 = new QAction("Set (Int)", this);
    action2_1->setStatusTip("Set a new integer value for the node.");
    QAction *action2_2 = new QAction("Set (Float)", this);
    action2_2->setStatusTip("Set a new floating point value for the node.");
    QAction *action2_3 = new QAction("Set (Bool)", this);
    action2_3->setStatusTip("Set a new boolean value for the node.");
    QAction *action2_4 = new QAction("Set (String)", this);
    action2_4->setStatusTip("Set a new string value for the node.");

    QAction *action3_1 = new QAction("Add (Folder)", this);
    action3_1->setStatusTip("Add a new folder node.");
    QAction *action3_2 = new QAction("Add (Variable)", this);
    action3_2->setStatusTip("Add a new variable node.");

    QAction *action4 = new QAction("Link", this);
    action4->setStatusTip("Link the selected node with the MQTT server.");
    QAction *action5 = new QAction("Unlink", this);
    action5->setStatusTip("Unlink the selected node from the MQTT server.");

    // Create menu
    QMenu menu(this);
    menu.addAction(action1);

    QMenu *menu_set = menu.addMenu("Set");
    menu_set->addAction(action2_1);
    menu_set->addAction(action2_2);
    menu_set->addAction(action2_3);
    menu_set->addAction(action2_4);

    QMenu *menu_add = menu.addMenu("Add");
    menu_add->addAction(action3_1);
    menu_add->addAction(action3_2);

    menu.addAction(action4);
    menu.addAction(action5);

    // Show menu at fixed pos
    QPoint pt(pos);
    pt.setY(pt.y() + menu.size().height() - 8);

    // Get the selected item
    QAction *selected = menu.exec(tree->mapToGlobal(pt));

    // Do what user wants
    if (selected)
    {
        if (selected == action1)
            treeShowNodeInfo(item);
        else if (selected == action2_1)
            treeSetNodeValue(item, OpcUa::VariantType::INT32);
        else if (selected == action2_2)
            treeSetNodeValue(item, OpcUa::VariantType::FLOAT);
        else if (selected == action2_3)
            treeSetNodeValue(item, OpcUa::VariantType::BOOLEAN);
        else if (selected == action2_4)
            treeSetNodeValue(item, OpcUa::VariantType::STRING);
        else if (selected == action3_1)
            treeAddNode(item, 0);
        else if (selected == action3_2)
            treeAddNode(item, 1);
        else if (selected == action4)
            createOpcUaMqttLink(item);
        else if (selected == action5)
            removeOpcUaMqttLink(item);
    }
}
