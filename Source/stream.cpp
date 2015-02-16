/*
***************************************************************************
*
* Author: Zach Swena
*
* Copyright (C) 2010, 2011, 2014 Zach Swena All Rights Reserved
*
* zcybercomputing@gmail.com
*
***************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation version 2 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
***************************************************************************
*
* This version of GPL is at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*
***************************************************************************
*/
#include "stream.h"
#include "windows.h"
/// ========================================================================================================
stream::stream(QObject *parent) :
    QObject(parent)
{
    qRegisterMetaType<QHostAddress>("QHostAddress");
    qRegisterMetaType<BufferList>("BufferList");

    worker = new Worker;

    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &stream::start_streaming , worker, &Worker::start_stream );
    connect(worker,SIGNAL(done_streaming()),this,SLOT(done_with_worker()));
    worker->moveToThread(&workerThread);
    workerThread.start(QThread::TimeCriticalPriority);
    //workerThread.start(QThread::HighestPriority);
    qDebug("Finished stream constructor");

}

stream::~stream()
{
    qDebug("stopping stream");
    worker->quit = true;
    workerThread.quit();
    workerThread.wait();
    qDebug("Stream destructor finished");
}

void stream::stream_start( QHostAddress stream_addr, quint16 stream_port, QString source_filename )
{
    emit start_streaming( stream_addr , stream_port , source_filename);
}

void stream::done_with_worker()
{
    emit done_with_stream();
    qDebug("done with stream signal sent");
}

Worker::Worker()
{
    packet_size = 188;
    pkts_per_dgm = 7;  // must be between 1 and 7 packets per datagram

    timer_period = 8*packet_size*pkts_per_dgm; // 8 bits per byte, ms between packets.
    timer_period = timer_period*1000000;
    // in stream function timer_period = timer_period/(kbit_rate);

    one_third_of_timer_period = timer_period/3;
    sleep_time = timer_period/2000;

    quit = false;
    qDebug("Constructor");
}

Worker::~Worker()
{
    qDebug("Closing Socket");
    qDebug("worker destructor finished");
}

void Worker::read_datagram()
{
    datagram.clear();
    for(int i=1; i<=pkts_per_dgm;i++)
    {
        bytes_read = readfile.read(packet,packet_size);
        if(bytes_read<=0)
            return;
        datagram.append( QByteArray( (char*) packet ,packet_size) );
    }
}

void Worker::start_stream( QHostAddress stream_addr , quint16 stream_port , QString source_filename)
{
    readfile.setFileName(source_filename);
    udp_streaming_socket = new QUdpSocket(this);

    if( readfile.open(QFile::ReadOnly) )
    {
        timer_period = timer_period/(4000);
        read_datagram();
        elapsed_timer.start();

        while( !datagram.isEmpty() && !quit)
        {
            while(elapsed_timer.nsecsElapsed() <= timer_period)
            {
                if( elapsed_timer.nsecsElapsed() <= one_third_of_timer_period )
                    QThread::usleep( sleep_time );
            }
            socket_state = udp_streaming_socket->writeDatagram( datagram.data() , datagram.size() ,stream_addr , stream_port);
            udp_streaming_socket->waitForBytesWritten();

            elapsed_timer.restart();

            if(socket_state!=(188*pkts_per_dgm))
            {
                while ( socket_state == -1 )
                {
                    socket_state = udp_streaming_socket->writeDatagram( datagram.data() , datagram.size() ,stream_addr , stream_port);
                    udp_streaming_socket->waitForBytesWritten();
                    QThread::usleep(1);
                }
            }
            emit datagram_sent(datagram);
            read_datagram();
        }
        readfile.close();
    }
    udp_streaming_socket->close();
}
