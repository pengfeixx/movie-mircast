/*
 * (c) 2017, Deepin Technology Co., Ltd. <support@deepin.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/**
 * @file 此文件中实现投屏搜索小窗口
 */
#ifndef _MIRCASTWIDGET_H
#define _MIRCASTWIDGET_H

#include <DWidget>
#include <DFloatingWidget>
#include <DLabel>

#include <QTimer>
#include <QScrollArea>

DWIDGET_USE_NAMESPACE

class QNetworkReply;
class QListWidget;
class CSSDPSearch;
class DlnaContentServer;
class CDlnaSoapPost;

class MircastWidget: public DFloatingWidget
{
    Q_OBJECT
public:
    enum MircastState {
        Searching,
        ListExhibit,
        NoDevices,
    };
public:
    MircastWidget(QWidget *mainWindow = nullptr, void *pEngine = nullptr);
//    virtual ~MircastWidget() override;

public slots:
    void togglePopup();
    void slotReadyRead();

private slots:
    void slotRefreshBtnClicked();
    void slotSearchTimeout();
    void slotConnectDevice(QModelIndex);
    void startDlnaTp();
    void pauseDlnaTp();
    void playDlnaTp();
    void seekDlnaTp(int nSeek);
    void getPosInfoDlnaTp();
signals:
    void closeServer();
private:
    /**
     * @brief searchDevices 刷新查找设备
     */
    void searchDevices();
    /**
     * @brief updateMircastState 更新投屏窗口状态
     */
    void updateMircastState(MircastState state);

    void createListeItem(QString, const QByteArray &data, const QNetworkReply*);
    //初始化http Sever
    void initializeHttpServer(int port = 9999);

private:
    QWidget     *m_hintWidget;
    DLabel      *m_hintLabel;
    QScrollArea *m_mircastArea;
    bool        m_bIsToggling;
//    MircastDevidesModel *m_mircastModel;
    QListWidget *m_listWidget;
    CSSDPSearch *m_search;

    QTimer          m_searchTime;
    QList<QString>  m_devicesList;
    //投屏http服务，支持http断点续传请求
    DlnaContentServer *m_dlnaContentServer;
    //投屏控制
    CDlnaSoapPost *m_pDlnaSoapPost;
    //是否成功启动http server
    bool m_isStartHttpServer;
    //投屏设备的控制url
    QString m_ControlURLPro;
    //本地准备的投屏主机地址
    QString m_URLAddrPro;
    //本地准备的投屏url地址
    QString m_sLocalUrl;
    void *m_pEngine;            ///播放引擎
};

#endif /* ifndef _MIRCASTWIDGET_H */
