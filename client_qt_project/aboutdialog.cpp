#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "config.h"
#include <sstream>
#include <QPixmap>

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    m_ui(new Ui::AboutDialog)
{
    // Basic UI setup
    m_ui->setupUi(this);
    setWindowTitle("About");
    QPixmap logo(":/res/icon.ico");
    m_ui->lb_logo->setPixmap(logo);
    m_ui->lb_logo->setScaledContents(true);
    std::ostringstream s;
    s << "OPC UA -> MQTT Gateway client\n" <<
         VERSION_S;
    m_ui->lb_about1->setText(std::string(s.str()).c_str());
    m_ui->lb_about2->setOpenExternalLinks(true);
    m_ui->lb_about2->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    m_ui->lb_about2->setText("<a href=\"https://github.com/harha/\">https://github.com/harha/</a>");

    // Signal -> Slot connections
    connect(m_ui->pb_close, SIGNAL(clicked(bool)),
            this, SLOT(close()));
}

AboutDialog::~AboutDialog()
{
    if (m_ui)
        delete m_ui;
}
