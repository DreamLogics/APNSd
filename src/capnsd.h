/****************************************************************************
**
** Apple Push Notification Service daemon
**
** Copyright (C) 2014 DreamLogics <info@dreamlogics.com>
** Copyright (C) 2014 Stefan Ladage <sladage@gmail.com>
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published
** by the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/
#ifndef CAPNSD_H
#define CAPNSD_H

#include <QObject>
#include <QString>
#include <QSslSocket>

struct SharedPayload;
class QTimer;
class QSharedMemory;
class QSocketNotifier;

class CAPNSd : public QObject
{
    Q_OBJECT
public:
    explicit CAPNSd(QSharedMemory *mem, SharedPayload *shared,bool bDaemon,QObject *parent = 0);
    ~CAPNSd();

    static void hupSignalHandler(int unused);
    static void termSignalHandler(int unused);

private:

    void log(int type, QString msg) const;

signals:

public slots:

    void setup();
    void handleSigHup();
    void handleSigTerm();

private slots:

    void checkPayloads();
    void encrypted();
    void disconnected();
    void connectSocket();
    void socketError(QAbstractSocket::SocketError);
    void sslErrors(const QList<QSslError> & errors);
    void readyRead();
    void checkFeedback();
    void readyReadFeedback();

private:
    QSharedMemory *m_pSharedMem;
    SharedPayload *m_pShared;
    QSslSocket *m_pSocket;
    QSslSocket *m_pFeedbackSocket;
    QTimer *m_pTimer;
    int m_iFailure;
    quint32 m_iIdent;
    bool m_bDaemon;

    static int m_sighupFd[2];
    static int m_sigtermFd[2];

    QSocketNotifier *m_psnHup;
    QSocketNotifier *m_psnTerm;
};

#endif // CAPNSD_H
