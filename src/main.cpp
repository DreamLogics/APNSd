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
#include <QCoreApplication>
#include <QSharedMemory>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include "capnsd.h"
#include "shared.h"
#include <QTimer>
#include <QString>
#include <QByteArray>

static int setup_unix_signal_handlers()
{
    struct sigaction hup, term;

    hup.sa_handler = CAPNSd::hupSignalHandler;
    sigemptyset(&hup.sa_mask);
    hup.sa_flags = 0;
    hup.sa_flags |= SA_RESTART;

    if (sigaction(SIGHUP, &hup, 0) > 0)
       return 1;

    term.sa_handler = CAPNSd::termSignalHandler;
    sigemptyset(&term.sa_mask);
    term.sa_flags |= SA_RESTART;

    if (sigaction(SIGTERM, &term, 0) > 0)
       return 2;

    return 0;
}

void usage()
{
    std::cout << "APNSd v0.1\n";
    std::cout << "APNSd push <device_id> <json string>; send push payload\n";
    std::cout << "APNSd d; start as daemon\n";
}

int main(int argc, char *argv[])
{
    bool bDaemon = false;

    if (argc >= 2)
    {
        if (strcmp(argv[1],"push") == 0)
        {
            if (argc != 4)
            {
                std::cout << "Missing payload or device identifier.\n";
                usage();
                return EXIT_FAILURE;
            }
            //std::string payloadstr(argv[2]);

            QSharedMemory payloadshare("APNSdShared");
            if (!payloadshare.attach())
            {
                std::cout << payloadshare.errorString().toStdString() << "\n";
                std::cout << "APNSd service not running?\n";
                return EXIT_FAILURE;
            }

            if (strlen(argv[2]) != 64)
            {
                std::cout << "Invalid device identifier.\n";
                return EXIT_FAILURE;
            }

            QByteArray jsonstrd(argv[3]);
            QString jsonstr = QString::fromUtf8(QByteArray::fromBase64(jsonstrd));

            if (strlen(jsonstr.toStdString().c_str()) >= PAYLOAD_JSONSTR_SIZE)
            {
                std::cout << "Payload is too large (PAYLOAD_JSONSTR_SIZE is max).\n";
                return EXIT_FAILURE;
            }

            SharedPayload *data = static_cast<SharedPayload*>(payloadshare.data());
            payloadshare.lock();

            if (data->size >= PAYLOAD_ARRAY_SIZE)
                std::cout << "Payload queue is full.\n";
            else
            {
                int index = data->size++;
                memcpy(data->data[index].device,argv[2],64);
                strcpy(data->data[index].json,jsonstr.toStdString().c_str());
            }

            payloadshare.unlock();
            payloadshare.detach();

            return EXIT_SUCCESS;
        }
        else if (strcmp(argv[1],"d") == 0)
        {
            bDaemon = true;
        }
        else
        {
            std::cout << "Invalid argument.\n";
            usage();
            return EXIT_FAILURE;
        }
    }


    if (bDaemon)
    {
        pid_t pid;

        //fork
        pid = fork();
        if (pid < 0) {
            return EXIT_FAILURE;
        }

        //exit parent
        if (pid > 0) {
            return EXIT_FAILURE;
        }

        /* Change the file mode mask */
        //umask(0);

        /* log file */
        openlog("APNSd",LOG_PID,LOG_DAEMON);

        /* Create a new SID for the child process */
        int sid = setsid();
        if (sid < 0) {
            syslog(LOG_ALERT, "Could not set SID.");
            return EXIT_FAILURE;
        }



        /* Change the current working directory */
        if ((chdir("/tmp/")) < 0) {
            syslog(LOG_ALERT, "Could not change directory.");
            return EXIT_FAILURE;
        }

        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

    }


    setup_unix_signal_handlers();

    QSharedMemory payloadshare("APNSdShared");

    if (!payloadshare.create(sizeof(SharedPayload)))
    {
        if (bDaemon)
            syslog(LOG_ALERT,payloadshare.errorString().toStdString().c_str());
        else
            std::cout << payloadshare.errorString().toStdString() << "\n";
        if (payloadshare.attach())
            payloadshare.detach();
        return EXIT_FAILURE;
    }

    SharedPayload *data = new (payloadshare.data()) SharedPayload;

    for (int i=0;i<PAYLOAD_ARRAY_SIZE;i++)
    {
        memset(data->data[i].device,0,64);
        memset(data->data[i].json,0,PAYLOAD_JSONSTR_SIZE);
    }

    QCoreApplication a(argc, argv);

    QCoreApplication::setApplicationName("APNSd");
    QCoreApplication::setApplicationVersion("0.1");

    CAPNSd server(&payloadshare,data,bDaemon);
    QTimer::singleShot(0,&server,SLOT(setup()));

    a.exec();
    payloadshare.detach();
    closelog();
    return EXIT_SUCCESS;

}
