#include "mircastwidget.h"
#include "player_engine.h"
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

#define MIRCASTWIDTH 240
#define MIRCASTHEIGHT 188
#define REFRESHTIME 20000
#define MAXMIRCAST 3

using namespace dmr;

MircastWidget::MircastWidget(QWidget *mainWindow, void *pEngine)
: DFloatingWidget(mainWindow), m_pEngine(pEngine)
{
    qRegisterMetaType<DlnaPositionInfo>("DlnaPositionInfo");
    setFixedSize(MIRCASTWIDTH + 14, MIRCASTHEIGHT + 10);
    m_bIsToggling = false;
    m_mircastState = Idel;
    m_attempts = 0;
    m_connectTimeout = 0;
    m_searchTime.setSingleShot(true);
    connect(&m_searchTime, &QTimer::timeout, this, &MircastWidget::slotSearchTimeout);
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

    m_listWidget = new QListWidget;
    m_listWidget->setFixedWidth(MIRCASTWIDTH);
    m_listWidget->setAttribute(Qt::WA_TranslucentBackground, true);
    m_listWidget->setFrameShape(QFrame::NoFrame);
    connect(m_listWidget, &QListWidget::doubleClicked, this, &MircastWidget::slotConnectDevice);
    m_listWidget->show();

    m_mircastArea = new QScrollArea;
    m_mircastArea->setFixedSize(MIRCASTWIDTH, 34 * 4);
    m_mircastArea->setFrameShape(QFrame::NoFrame);
    m_mircastArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mircastArea->setWidget(m_listWidget);

//    m_mircastModel = new MircastDevidesModel;
//    MircastDevicesItem *mircastItem = new MircastDevicesItem;
//    m_listWidget->setModel(m_mircastModel);
//    m_listWidget->setItemDelegate(mircastItem);

    mainLayout->addWidget(m_mircastArea);
    m_mircastArea->hide();
    m_dlnaContentServer = nullptr;
    m_search = new CSSDPSearch(this);
    m_pDlnaSoapPost = new CDlnaSoapPost(this);
    connect(m_pDlnaSoapPost, &CDlnaSoapPost::sigGetPostionInfo, this, &MircastWidget::slotGetPositionInfo, Qt::QueuedConnection);
}

MircastWidget::MircastState MircastWidget::getMircastState()
{
    return m_mircastState;
}

void MircastWidget::playNext()
{
    if (m_mircastState != MircastState::Idel) {
        PlayerEngine *engine =static_cast<PlayerEngine *>(m_pEngine);
        engine->pauseResume();
        stopDlnaTP();
        startDlnaTp();
    }
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
        updateMircastState(SearchState::NoDevices);
    else
        updateMircastState(SearchState::ListExhibit);
}

void MircastWidget::slotMircastTimeout()
{
    m_pDlnaSoapPost->SoapOperPost(DLNA_GetPositionInfo, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
    m_connectTimeout++;
    if (m_connectTimeout >= 3)
        emit mircastState(-4, "");
}

void MircastWidget::slotGetPositionInfo(DlnaPositionInfo info)
{
    if (m_mircastState == MircastState::Idel)
        return;

    m_connectTimeout--;
    PlayerEngine *engine =static_cast<PlayerEngine *>(m_pEngine);
    PlaylistModel *model = engine->getplaylist();
    PlaylistModel::PlayMode playMode = model->playMode();

    if (m_mircastState == MircastState::Screening) {
        int absTime = timeConversion(info.sAbsTime);
        updateTime(absTime);
        if (info.sAbsTime == info.sTrackDuration) {
            m_mircastState = Connecting;
            m_attempts = MAXMIRCAST;
        }
        return;
    }
    int duration = timeConversion(info.sTrackDuration);
    int absTime = timeConversion(info.sAbsTime);
    if (duration > 0 && absTime > 0) {
        emit mircastState(0, m_devicesList.at(0));
        m_mircastState = MircastState::Screening;
        m_attempts = 0;
    } else {
        if (duration > 0)
            emit mircastState(0, m_devicesList.at(0));
        qWarning() << "mircast failed!";
        if (m_attempts >= MAXMIRCAST) {
            qWarning() << "attempts time out! try next.";
            m_attempts = 0;
            m_mircastTimeOut.stop();
//            if (duration > 0 && absTime <= 0)
//                emit mircastState(-3, QString(""));
            if (playMode == PlaylistModel::SinglePlay ||
                    (playMode == PlaylistModel::OrderPlay && model->current() == (model->count() - 1))) {
                emit mircastState(-1, QString(""));
                slotExitMircast();
            } else if (playMode == PlaylistModel::SingleLoop) {
                startDlnaTp();
            } else {
                emit mircastState(-3, QString(""));
                model->playNext(true);
                m_mircastState = Connecting;
            }
        } else {
            qInfo() << "mircast failed! curret attempts:" << m_attempts << "Max:" << MAXMIRCAST;
            m_attempts++;
            m_mircastState = Connecting;
        }
    }
}

void MircastWidget::slotConnectDevice(QModelIndex index)
{
    int ind = index.row();
    pauseDlnaTp();
}

void MircastWidget::slotStartMircast()
{
    PlayerEngine *engine =static_cast<PlayerEngine *>(m_pEngine);
    if (engine->state() == PlayerEngine::CoreState::Idle) {
        return;
    }
    //TODO：这里需要判断当前投屏设备与选择设备是否一致，不一致切换投屏设备
    if (m_mircastState == MircastState::Screening)
        return;
    m_mircastState = Connecting;
    startDlnaTp();
}

void MircastWidget::searchDevices()
{
    qInfo() << __func__ << "start search Devices!";
    m_devicesList.clear();
    static_cast<QListWidget *>(m_mircastArea->widget())->clear();
    m_searchTime.start(REFRESHTIME);
    m_search->SsdpSearch();
    updateMircastState(SearchState::Searching);
}

void MircastWidget::updateMircastState(MircastWidget::SearchState state)
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
    updateMircastState(SearchState::ListExhibit);
}

void MircastWidget::slotExitMircast()
{
    qInfo() << __func__ << "Exit Mircast.";
    if (m_mircastState == Idel)
        return;
    m_mircastState = Idel;
    m_connectTimeout = 0;
    stopDlnaTP();
    //    emit closeServer();
}

void MircastWidget::slotSeekMircast(int seek)
{
    seekDlnaTp(seek);
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
    if (btn) {
        m_ControlURLPro = btn->property(controlURLPro).toString();
        m_URLAddrPro = btn->property(urlAddrPro).toString();
    }
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
//    if(btn->text() != "Stop") {
        m_pDlnaSoapPost->SoapOperPost(DLNA_Stop, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
        m_pDlnaSoapPost->SoapOperPost(DLNA_SetAVTransportURI, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
//        btn->setText("Stop");
//    } else {
//        m_pDlnaSoapPost->SoapOperPost(DLNA_Stop, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
//        btn->setText(btn->property(friendlyNamePro).toString());
//    }

    m_mircastTimeOut.start(1000);
    m_attempts++;
}

int MircastWidget::timeConversion(QString time)
{
    QStringList timeList = time.split(":");
    if (timeList.size() == 3) {
        int realTime = 0;
        realTime += timeList.at(0).toInt() * 60 * 60;
        realTime += timeList.at(1).toInt() * 60;
        realTime += timeList.at(2).toInt();

        return realTime;
    }
    return 0;
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

void MircastWidget::stopDlnaTP()
{
    m_pDlnaSoapPost->SoapOperPost(DLNA_Stop, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
}

void MircastWidget::getPosInfoDlnaTp()
{
    m_pDlnaSoapPost->SoapOperPost(DLNA_GetPositionInfo, m_ControlURLPro, m_URLAddrPro, m_sLocalUrl);
}
