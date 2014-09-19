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
#include "capnsd.h"
#include "shared.h"
#include <unistd.h>
#include <iostream>
#include <QSettings>
#include <QSslSocket>
#include <syslog.h>
#include <QTimer>
#include <QSharedMemory>
#include <QDataStream>
#include <QFile>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <sys/types.h>
#include <sys/socket.h>
#include <QDateTime>

int CAPNSd::m_sighupFd[];
int CAPNSd::m_sigtermFd[];

CAPNSd::CAPNSd(QSharedMemory *mem,SharedPayload *shared,bool bDaemon,QObject *parent) :
    QObject(parent), m_pSharedMem(mem), m_pShared(shared), m_bDaemon(bDaemon)
{
    m_iFailure = 0;
    m_iIdent = 0;
    m_pTimer = new QTimer();
    m_pTimer->setInterval(500);
#if QT_VERSION >= 0x050000
    m_pTimer->setTimerType(Qt::CoarseTimer);
#endif
    connect(m_pTimer,SIGNAL(timeout()),this,SLOT(checkPayloads()));

    m_pSocket = new QSslSocket();
    m_pFeedbackSocket = new QSslSocket();

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, m_sighupFd))
        qFatal("Couldn't create HUP socketpair");

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, m_sigtermFd))
        qFatal("Couldn't create TERM socketpair");
    m_psnHup = new QSocketNotifier(m_sighupFd[1], QSocketNotifier::Read, this);
    connect(m_psnHup, SIGNAL(activated(int)), this, SLOT(handleSigHup()));
    m_psnTerm = new QSocketNotifier(m_sigtermFd[1], QSocketNotifier::Read, this);
    connect(m_psnTerm, SIGNAL(activated(int)), this, SLOT(handleSigTerm()));
}

CAPNSd::~CAPNSd()
{
    m_pSocket->deleteLater();
    m_pTimer->deleteLater();
}

void CAPNSd::setup()
{
    QSettings settings("/etc/APNSd.cfg",QSettings::IniFormat);

    if (!(settings.contains("local_cert_file") && settings.contains("private_key_passprase") && settings.contains("private_key_file")
            && settings.contains("apns_server") && settings.contains("apns_server_port") && settings.contains("root_cert_file")))
    {
        settings.setValue("local_cert_file",QVariant("/sslcerts/cert.pem"));
        settings.setValue("root_cert_file",QVariant("/sslcerts/entrust_2048_ca.cer"));
        settings.setValue("private_key_file",QVariant("/sslcerts/pk.pem"));
        settings.setValue("private_key_passprase",QVariant(QByteArray("1234")));
        settings.setValue("apns_server",QVariant("gateway.sandbox.push.apple.com"));
        settings.setValue("apns_server_port",QVariant(2195));
        settings.sync();

        log(LOG_ALERT,"Missing settings. Settings file recreated.");

        qApp->exit(EXIT_FAILURE);
        return;
    }

    if (!QFile(settings.value("root_cert_file").toString()).exists())
    {
        QString err = "Could not find root ca certificate file. (";
        err += settings.value("root_cert_file").toString() + ")";
        log(LOG_ALERT,err);

        qApp->exit(EXIT_FAILURE);
        return;
    }


    if (!QFile(settings.value("local_cert_file").toString()).exists())
    {
        QString err = "Could not find local certificate file. (";
        err += settings.value("local_cert_file").toString() + ")";
        log(LOG_ALERT,err);
        qApp->exit(EXIT_FAILURE);
        return;
    }

    if (!QFile(settings.value("private_key_file").toString()).exists())
    {
        QString err = "Could not find private key file. (";
        err += settings.value("private_key_file").toString() + ")";
        log(LOG_ALERT,err);
        qApp->exit(EXIT_FAILURE);
        return;
    }

    QList<QSslCertificate> cert = QSslCertificate::fromPath(settings.value("local_cert_file").toString());
    QList<QSslCertificate> cacert = QSslCertificate::fromPath(settings.value("root_cert_file").toString());

    m_pSocket->addCaCertificates(cacert);
    m_pSocket->setLocalCertificate(cert[0]);
    m_pSocket->ignoreSslErrors(/*expectedSslErrors*/);
    m_pSocket->setPrivateKey(settings.value("private_key_file").toString(),QSsl::Rsa,QSsl::Pem,settings.value("private_key_passprase").toByteArray());

    m_pSocket->setPeerVerifyMode(QSslSocket::QueryPeer);

    m_pFeedbackSocket->addCaCertificates(cacert);
    m_pFeedbackSocket->setLocalCertificate(cert[0]);
    m_pFeedbackSocket->ignoreSslErrors(/*expectedSslErrors*/);
    m_pFeedbackSocket->setPrivateKey(settings.value("private_key_file").toString(),QSsl::Rsa,QSsl::Pem,settings.value("private_key_passprase").toByteArray());

    m_pFeedbackSocket->setPeerVerifyMode(QSslSocket::QueryPeer);

    connect(m_pSocket,SIGNAL(encrypted()),this,SLOT(encrypted()));
    connect(m_pSocket,SIGNAL(disconnected()),this,SLOT(disconnected()));
    connect(m_pSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(socketError(QAbstractSocket::SocketError)));
    connect(m_pSocket,SIGNAL(sslErrors(QList<QSslError>)),this,SLOT(sslErrors(QList<QSslError>)));
    connect(m_pSocket,SIGNAL(readyRead()),this,SLOT(readyRead()));

    connect(m_pFeedbackSocket,SIGNAL(readyRead()),this,SLOT(readyReadFeedback()));

    connectSocket();
}

void CAPNSd::encrypted()
{
    log(LOG_INFO,"Connected to APN service.");

    m_iFailure = 0;
    m_pTimer->start();
}

void CAPNSd::disconnected()
{
    m_pTimer->stop();
    m_iIdent = 0;
    if (m_iFailure++ > 3)
    {
        log(LOG_ALERT,"Could not connect to APN service. Retry in 30 seconds...");
#if QT_VERSION >= 0x050000
        QTimer::singleShot(30000,Qt::VeryCoarseTimer,this,SLOT(connectSocket()));
#else
        QTimer::singleShot(30000,this,SLOT(connectSocket()));
#endif
        return;
    }
    //reconnect
    log(LOG_ALERT,"Connection reset.");
    connectSocket();
}

void CAPNSd::connectSocket()
{
    QSettings settings("/etc/APNSd.cfg",QSettings::IniFormat);
    QString msg = "Connecting to "+settings.value("apns_server").toString()+":"+settings.value("apns_server_port").toString()+"...";
    log(LOG_INFO,msg);
    m_pSocket->connectToHostEncrypted(settings.value("apns_server").toString(),settings.value("apns_server_port").toInt());
}

void CAPNSd::checkPayloads()
{
    m_pSharedMem->lock();

    if (m_pShared->size == 0)
    {
        m_pSharedMem->unlock();
        return;
    }

    QString msg = "Sending " + QString::number(m_pShared->size) + " push payloads.";

    log(LOG_INFO,msg);

#ifdef PUSH_PROTOCOL_V2
    QByteArray data;
    QDataStream ds(&data,QIODevice::WriteOnly);
    quint32 size = 0;
    ds << (quint8)(2) << (quint32)(0);

    for (quint8 i=0;i<m_pShared->size;i++)
    {
        QByteArray device = QByteArray::fromHex(QByteArray(m_pShared->data[i].device,64));
        QString json= QString::fromUtf8(m_pShared->data[i].json);

        memset(m_pShared->data[i].json,0,PAYLOAD_JSONSTR_SIZE);

        m_iIdent++;

        ds << i << (quint16)(32 + json.size() + 9);
        ds.writeRawData(device.data(),32);
        ds.writeRawData(json.toUtf8().data(),json.size());
        ds << m_iIdent << (quint32)(0) << (quint8)(10);
        size += 3 + 32 + json.size() + 9;
    }

    m_pShared->size = 0;

    m_pSharedMem->unlock();

    ds.device()->seek(1);
    ds << size;

    QFile test("test");
    test.open(QIODevice::WriteOnly);
    test.write(data);
    test.close();
    m_pSocket->write(data);
#else //push protocol v0
    for (quint8 i=0;i<m_pShared->size;i++)
    {
        QByteArray data;
        QDataStream ds(&data,QIODevice::WriteOnly);

        ds << (quint8)(0) << (quint16)(32);

        QByteArray device = QByteArray::fromHex(QByteArray(m_pShared->data[i].device,64));
        QString json= QString::fromUtf8(m_pShared->data[i].json);

        memset(m_pShared->data[i].json,0,PAYLOAD_JSONSTR_SIZE);

        ds.writeRawData(device.data(),32);
        ds << (quint16)(json.size());
        ds.writeRawData(json.toUtf8().data(),json.size());
        //qDebug() << data;
        m_pSocket->write(data);
    }

    m_pShared->size = 0;

    m_pSharedMem->unlock();
#endif
}

void CAPNSd::socketError(QAbstractSocket::SocketError err)
{
    QString msg = "Socket error: "+QString::number(err);
    log(LOG_ALERT,msg);
}

void CAPNSd::sslErrors(const QList<QSslError> &errors)
{
    for (int i=0;i<errors.size();i++)
        log(LOG_ALERT,errors[i].errorString());
}

void CAPNSd::readyRead()
{
    if (m_pSocket->bytesAvailable() == 0)
        return;
    QString str = "APNS reply: ";
    //syslog(LOG_ALERT,str.toStdString().c_str());
    QByteArray d = m_pSocket->readAll();
    QDataStream ds(&d,QIODevice::ReadWrite);
    quint8 cmd,status;
    quint32 id;
    ds >> cmd >> status >> id;

    if (cmd == 8)
    {
        if (status == 0)
            str += "No errors";
        else if (status == 1)
            str += "Processing error";
        else if (status == 2)
            str += "Missing device token";
        else if (status == 3)
            str += "Missing topic";
        else if (status == 4)
            str += "Missing payload";
        else if (status == 5)
            str += "Invalid token size";
        else if (status == 6)
            str += "Invalid topic size";
        else if (status == 7)
            str += "Invalid payload size";
        else if (status == 8)
            str += "Invalid token";
        else if (status == 10)
            str += "Shutdown";
        else
            str += "Unknown error ("+QString::number(status)+")";

        str += " For id " + QString::number(id);
    }
    else
        str += "Unknown command";
    log(LOG_ALERT,str);
    //checkFeedback();
}

void CAPNSd::readyReadFeedback()
{
    if (m_pFeedbackSocket->bytesAvailable() == 0)
        return;
    QString str = "APNS reply: ";
    //syslog(LOG_ALERT,str.toStdString().c_str());
    QByteArray d = m_pFeedbackSocket->readAll();
    QDataStream ds(&d,QIODevice::ReadWrite);
    quint32 time;
    quint16 tokenlen;

    while (!ds.atEnd())
    {
        ds >> time >> tokenlen;
        char* tokenbuffer = new char[tokenlen];
        ds.readRawData(tokenbuffer,tokenlen);
        QByteArray token(tokenbuffer,tokenlen);
        delete []tokenbuffer;
        token = token.toHex();
        //qDebug() << "invalid token: " << token;
    }

}

void CAPNSd::checkFeedback()
{
    QSettings settings("/etc/APNSd.cfg",QSettings::IniFormat);
    QString serv = settings.value("apns_server").toString();
    serv.replace("gateway","feedback");
    QString msg = "Connecting to "+serv+":"+QString::number(settings.value("apns_server_port").toInt()+1)+"...";
    log(LOG_INFO,msg);
    m_pFeedbackSocket->connectToHostEncrypted(serv,settings.value("apns_server_port").toInt()+1);
}

void CAPNSd::hupSignalHandler(int)
 {
     char a = 1;
     ::write(m_sighupFd[0], &a, sizeof(a));
 }

 void CAPNSd::termSignalHandler(int)
 {
     char a = 1;
     ::write(m_sigtermFd[0], &a, sizeof(a));
 }

 void CAPNSd::handleSigTerm()
{
    m_psnTerm->setEnabled(false);
    char tmp;
    ::read(m_sigtermFd[1], &tmp, sizeof(tmp));

    qApp->quit();

    m_psnTerm->setEnabled(true);
}

void CAPNSd::handleSigHup()
{
    m_psnHup->setEnabled(false);
    char tmp;
    ::read(m_sighupFd[1], &tmp, sizeof(tmp));

    qApp->quit();

    m_psnHup->setEnabled(true);
}

void CAPNSd::log(int type, QString msg) const
{
    if (m_bDaemon)
        syslog(type,msg.toStdString().c_str());
    else
    {
        QDateTime d;
        if (type == LOG_ALERT)
            std::cout << d.currentDateTime().toString("dd-MM-yyyy HH:mm:ss").toStdString()  <<  " - error: " << msg.toStdString() << "\n";
        else
            std::cout << d.currentDateTime().toString("dd-MM-yyyy HH:mm:ss").toStdString()  <<  " - info: " << msg.toStdString() << "\n";
    }
}
