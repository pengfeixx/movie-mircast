#include "mircastwidget.h"
#include "dlna/cssdpsearch.h"
#include "dlna/getdlnaxmlvalue.h"
#include "dlna/dlnacontentserver.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QAbstractListModel>
#include <QNetworkReply>
#include <QFileInfo>
#include <QNetworkInterface>
#include <DPushButton>
#include "player_engine.h"

#define MIRCASTWIDTH 240
#define MIRCASTHEIGHT 188
#define REFRESHTIME 20000
#define MAXMIRCAST 5

MircastWidget::MircastWidget(QWidget *mainWindow, void *pEngine)
: DFloatingWidget(mainWindow), m_pEngine(pEngine)
{
    qRegisterMetaType<DlnaPositionInfo>("DlnaPositionInfo");
    setFixedSize(MIRCASTWIDTH + 14, MIRCASTHEIGHT + 10);
    m_bIsToggling = false;
    m_searchTime.setSingleShot(true);
    connect(&m_searchTime, &QTimer::timeout, this, &MircastWidget::slotSearchTimeout);

    m_mircastTimeOut.setSingleShot(true);
    connect(&m_mircastTimeOut, &QTimer::timeout, this, &MircastWidget::slotMircastTimeout);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);
    setContentsMargins(0, 0, 0, 0);

    QWidget *topWdiget = new QWidget(this);
    topWdiget->setFixedHeight(42);
    mainLayout->addWidget(topWdiget);

    DLabel *projet = new DLabel(topWdiget);
    projet->setText(tr("Project to"));
    projet->move(20, 8);

    DPushButton *refreshBtn = new DPushButton(topWdiget);
    refreshBtn->setFixedSize(24, 24);
    refreshBtn->move(206, 8);
    connect(refreshBtn, &DPushButton::clicked, this, &MircastWidget::slotRefreshBtnClicked);

    QFrame *spliter = new QFrame(this);
    spliter->setAutoFillBackground(true);
    spliter->setPalette(QPalette(QColor(0, 0, 0, 13)));
    spliter->setFixedSize(MIRCASTWIDTH, 2);
    mainLayout->addWidget(spliter);

    m_hintWidget = new QWidget(this);
    mainLayout->addWidget(m_hintWidget);
    m_hintWidget->setFixedSize(MIRCASTWIDTH, MIRCASTHEIGHT - 42);
    QVBoxLayout *hintLayout = new QVBoxLayout(m_hintWidget);
    m_hintWidget->setLayout(hintLayout);

    m_hintLabel = new DLabel(this);
    m_hintLabel->setAlignment(Qt::AlignVCenter);
    m_hintLabel->setFixedSize(MIRCASTWIDTH, MIRCASTHEIGHT - 42);
    m_hintLabel->setWordWrap(true);
    hintLayout->addWidget(m_hintLabel);
    m_hintLabel->hide();

    m_mircastArea = new QScrollArea(this);
    m_listWidget = new QListWidget(m_mircastArea);
//    m_mircastModel = new MircastDevidesModel;
//    MircastDevicesItem *mircastItem = new MircastDevicesItem;

//    m_listWidget->setModel(m_mircastModel);
//    m_listWidget->setItemDelegate(mircastItem);

    m_listWidget->setFixedWidth(MIRCASTWIDTH);
    m_listWidget->setAttribute(Qt::WA_TranslucentBackground, true);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    m_listWidget->show();
    connect(m_listWidget, &QListWidget::doubleClicked, this, &MircastWidget::slotConnectDevice);
    m_mircastArea->setFixedSize(MIRCASTWIDTH, 34 * 4);
    m_mircastArea->setAttribute(Qt::WA_TranslucentBackground, true);
    m_mircastArea->setWidget(m_listWidget);
    mainLayout->addWidget(m_mircastArea);
    m_mircastArea->hide();
    m_dlnaContentServer = nullptr;
    m_search = new CSSDPSearch(this);
    m_pDlnaSoapPost = new CDlnaSoapPost(this);
    connect(m_pDlnaSoapPost, &CDlnaSoapPost::sigGetPostionInfo, this, &MircastWidget::slotGetPositionInfo);
}


void MircastWidget::togglePopup()
{
    if (m_bIsToggling) return;
    if (isVisible()) {
        hide();
    } else {
        m_bIsToggling = true;
        show();
        raise();
        m_bIsToggling = false;
    }
}

void MircastWidget::slotRefreshBtnClicked()
{
    initializeHttpServer();
    searchDevices();
}

void MircastWidget::slotSearchTimeout()
{
    qInfo() << "search timeout!!";
    if (m_devicesList.isEmpty())
        updateMircastState(MircastState::NoDevices);
    else
        updateMircastState(MircastState::ListExhibit);
}

void MircastWidget::slotMircastTimeout()
{
    m_pDlnaSoapPost->SoapOperPost(DLNA_GetPositionInfo, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
}

void MircastWidget::slotGetPositionInfo(DlnaPositionInfo info)
{
    //debug
    emit mircastState(0, m_devicesList.at(0));

//    if (info.sRelTime.toInt()) {
//        emit mircastState(0);
//    } else {
//        qWarning() << "mircast failed!";
//        m_mircastTimeOut.start(1000);
//        emit mircastState(-1);
//    }
}

void MircastWidget::slotConnectDevice(QModelIndex index)
{
    int ind = index.row();
    pauseDlnaTp();
}

void MircastWidget::slotStartMircast()
{
    startDlnaTp();
}

void MircastWidget::searchDevices()
{
    qInfo() << __func__ << "start search Devices!";
    m_devicesList.clear();
    static_cast<QListWidget *>(m_mircastArea->widget())->clear();
    m_searchTime.start(REFRESHTIME);
    m_search->SsdpSearch();
    updateMircastState(MircastState::Searching);
}

void MircastWidget::updateMircastState(MircastWidget::MircastState state)
{
    switch (state) {
    case Searching:
        if (!m_devicesList.isEmpty())
            return;
        m_hintLabel->setText(tr("Searching for devices..."));
        m_mircastArea->hide();
        m_hintLabel->show();
        break;
    case ListExhibit:
        m_hintWidget->hide();
        m_hintLabel->hide();
        m_mircastArea->show();
        break;
    case NoDevices:
        m_hintLabel->setText(tr("No Mircast display devices were found. Please connect the device and your computer to the same WLAN network."));
        m_mircastArea->hide();
        m_hintLabel->show();
        break;
    }
}

void MircastWidget::createListeItem(QString name, const QByteArray &data, const QNetworkReply *reply)
{
//    DlnaPositionInfo info;
//    info.sTrackMetaData = name;

//    m_mircastModel->addItem(info);
//    m_mircastModel->refrushModel();
//    m_listWidget->update(m_mircastModel->index(m_mircastModel->rowCount() - 1));
//    m_listWidget->viewport()->update();



    QWidget *itemWidget = new QWidget;
    QListWidgetItem *listItem = new QListWidgetItem;
    listItem->setSizeHint(QSize(MIRCASTWIDTH, 34));

    QHBoxLayout *itemLayout = new QHBoxLayout;
    itemWidget->setLayout(itemLayout);
    DLabel *deviceName = new DLabel(itemWidget);
    deviceName->setObjectName("ItemTextLabel");
    deviceName->setText(name);
    itemLayout->addWidget(deviceName);

    DPushButton *connectBtn = new DPushButton(itemWidget);
    connect(connectBtn, &DPushButton::clicked, this, &MircastWidget::slotStartMircast);
    connectBtn->setFixedSize(24, 24);
    itemLayout->addWidget(connectBtn);

    GetDlnaXmlValue dlnaxml(data);
    QString sName = dlnaxml.getValueByPath("device/friendlyName");
    connectBtn->setProperty(urlAddrPro, reply->property(urlAddrPro).toString());
    QString strControlURL = dlnaxml.getValueByPathValue("device/serviceList", "serviceType=urn:schemas-upnp-org:service:AVTransport:1", "controlURL");
    if(!strControlURL.startsWith("/")) {
        connectBtn->setProperty(controlURLPro, reply->property(urlAddrPro).toString() + "/" +strControlURL);
    } else {
        connectBtn->setProperty(controlURLPro, reply->property(urlAddrPro).toString() +strControlURL);
    }
    connectBtn->setProperty(friendlyNamePro, sName);

    listItem->setToolTip(name);
    static_cast<QListWidget *>(m_mircastArea->widget())->addItem(listItem);
    static_cast<QListWidget *>(m_mircastArea->widget())->setItemWidget(listItem, itemWidget);
    m_listWidget->resize(MIRCASTWIDTH, m_listWidget->count() * 34);
}

void MircastWidget::slotReadyRead()
{
    QNetworkReply *reply = (QNetworkReply *)sender();
    if(reply->error() != QNetworkReply::NoError) {
//        QMessageBox::warning(this,"Error", QString::number(reply->error()));
        qInfo() << "Error:" << QString::number(reply->error());
        return;
    }
    QByteArray data = reply->readAll().replace("\r\n", "").replace("\\", "");
    qInfo() << "xml data:" << data;
    GetDlnaXmlValue dlnaxml(data);
    QString sName = dlnaxml.getValueByPath("device/friendlyName");
    m_devicesList.append(sName);

//    int nNum = reply->property(replayShowNum).toInt();
//    if(nNum >= 20) return;
//    QPushButton *btn = m_listBtn.at(nNum);
//    btn->setText(sName);
//    btn->setToolTip(reply->property(urlAddrPro).toString());
//    btn->show();
////    QPushButton *btn = new QPushButton("荣耀智慧屏");
//    btn->setProperty(urlAddrPro, reply->property(urlAddrPro).toString());
//    QString strControlURL = dlnaxml.getValueByPathValue("device/serviceList", "serviceType=urn:schemas-upnp-org:service:AVTransport:1", "controlURL");
//    if(!strControlURL.startsWith("/")) {
//        btn->setProperty(controlURLPro, reply->property(urlAddrPro).toString() + "/" +strControlURL);
//    } else {
//        btn->setProperty(controlURLPro, reply->property(urlAddrPro).toString() +strControlURL);
//    }
//    btn->setProperty(friendlyNamePro, sName);


//    ui->textEdit->append(btn->property(controlURLPro).toString());
    createListeItem(sName, data, reply);
    updateMircastState(MircastState::ListExhibit);
}

void MircastWidget::initializeHttpServer(int port)
{
    if(!m_dlnaContentServer) {
        QList<QHostAddress> lstInfo = QNetworkInterface::allAddresses();
        QString sLocalIp;
        foreach(QHostAddress address, lstInfo)
        {
             if(address.protocol() == QAbstractSocket::IPv4Protocol && !address.toString().trimmed().contains("127.0."))
             {
                 sLocalIp =  address.toString().trimmed();
                 break;
             }
        }
        m_dlnaContentServer = new DlnaContentServer(NULL, port);
        connect(this, &MircastWidget::closeServer, m_dlnaContentServer, &DlnaContentServer::closeServer);


        m_dlnaContentServer->setBaseUrl(QString("http://%1:%2/").arg(sLocalIp, QString::number(port)));
    }
}

void MircastWidget::startDlnaTp()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    m_ControlURLPro = btn->property(controlURLPro).toString();
    m_URLAddrPro = btn->property(urlAddrPro).toString();
    if(!m_dlnaContentServer)
    {
        qInfo() << "note: please Create httpServer!";
        return;
    } else {
        dmr::PlayerEngine *pEngine = static_cast<dmr::PlayerEngine *>(m_pEngine);
        if(pEngine && pEngine->playlist().currentInfo().url.isLocalFile()) {
            m_dlnaContentServer->setDlnaFileName(pEngine->playlist().currentInfo().url.toLocalFile());
            m_sLocalUrl = m_dlnaContentServer->getBaseUrl()  + QFileInfo(pEngine->playlist().currentInfo().url.toLocalFile()).fileName();
        }
        m_isStartHttpServer = m_dlnaContentServer->getIsStartHttpServer();
    }
    if(!m_isStartHttpServer)
    {
        qInfo() << "note: please start httpServer!";
        return;
    }
    if(btn->text() != "Stop") {
        m_pDlnaSoapPost->SoapOperPost(DLNA_Stop, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
        m_pDlnaSoapPost->SoapOperPost(DLNA_SetAVTransportURI, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
        btn->setText("Stop");
    } else {
        m_pDlnaSoapPost->SoapOperPost(DLNA_Stop, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
        btn->setText(btn->property(friendlyNamePro).toString());
    }

    m_mircastTimeOut.start(1000);
}

void MircastWidget::pauseDlnaTp()
{
    m_pDlnaSoapPost->SoapOperPost(DLNA_Pause, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
}

void MircastWidget::playDlnaTp()
{
    m_pDlnaSoapPost->SoapOperPost(DLNA_Play, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
}

void MircastWidget::seekDlnaTp(int nSeek)
{
    m_pDlnaSoapPost->SoapOperPost(DLNA_Seek, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl, nSeek);
}

void MircastWidget::getPosInfoDlnaTp()
{
    m_pDlnaSoapPost->SoapOperPost(DLNA_GetPositionInfo, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
}
