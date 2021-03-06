#include "ts_info.h"
#include "windows.h"
#include "QDebug"
#include "QtCore"
TS_Info::TS_Info(QObject *parent) :
    QObject(parent)
{
    kill = false;
    //qDebug("constructor");
}

TS_Info::~TS_Info()
{
    // Destructor
}

Transport_Stream TS_Info::process_file (QString source_filename)
{
    log("analysing " + source_filename) ;
    QByteArray ts_packet;
    char packet[188];
    int bytes_read = 188;
    int packets_read = 0;
    int pcr_read = 1;
    int bytes = 0;
    int last_pcr_packet = 0;
    int packets_since_pcr = 0;
    quint64 kbitrate = 0;
    quint64 last_pcr = 0;
    quint64 ms_between_pcr = 0;
    quint64 delta_pcr = 0;
    QFile file(source_filename);
    quint8  pid                    = 0;
    quint8  adaptFlag              = 0;
    quint8  pcrFlag                = 0;
    QString packet_info;
    QStringList info_list;
    PCR pcr;
    int pcr_count;
    pcr_count = 50;
    int bitrate_count;
    bitrate_count = 0;
    quint64 msec_per_pkt = 5;
    ts_properties.kbitrate = 0;
        while(!file.open(QIODevice::ReadOnly))
        {
            if(kill)
            {
                qDebug("kill");
                log("Analysis Failed") ;
                return(ts_properties);
            }
           // qDebug("File not opened");
            QThread::msleep(100);
        }
        QThread::msleep(50);
        packets_read=0;

        while(pcr_read<pcr_count)
        {
            if(kill)
            {
                qDebug("kill");
                log("Analysis Failed");
                return(ts_properties);
            }
           // qDebug("reading packets");
            QThread::msleep( msec_per_pkt+1 );
            ts_packet.clear();
            bytes_read  = file.read(packet,188);
            packets_read++;
            ts_packet.append( QByteArray((char*)packet,188) );

            pid  = (ts_packet.at(1) & 0x1F) << 8;                // 0x1F = 0001 1111;  TEI, Pusi, Transport Priority, PID Hi 5 bits
            pid |=  ts_packet.at(2);                             // OR in Low 8 bits
            adaptFlag = ts_packet.at(3) & 0x20;	     // 0x20 = 0010 0000 = Scramble 00;  Adapt 10, Payload 01;  Continuity Count
            //adaptLength = ts_packet.at(4);
            pcrFlag = ts_packet.at(5) & 0x10;			          // 0x10 = 0001 0000   3 Flags, PCR Flag, 4 Flags
            // vidCc = ts_packet.at(3) & 0x0F;                        // Get first Continuity Count from Global ts_packet[]

            if (adaptFlag == 0x20)
            {
                // Adaption field is present
                if (pcrFlag == 0x10)
                {
                    packet_info.clear();
                    pcr = getPCR( ts_packet );
                    pcr_read++;
                    packets_since_pcr = packets_read-2 - last_pcr_packet;
                    if(pcr_read>2)
                    {
                        bitrate_count++;
                        //qDebug()<< packets_since_pcr;
                        bytes = packets_since_pcr*188;
                        delta_pcr = pcr.composite - last_pcr;
                        ms_between_pcr = delta_pcr/27000;
                        //qDebug() << "ms" << ms_between_pcr;
                        msec_per_pkt = ms_between_pcr/packets_since_pcr;
                        kbitrate = kbitrate + (8*bytes)/ms_between_pcr;
                    }

                    if(pcr_read==pcr_count)
                    {
                       // qDebug()<< "bitrate count " << bitrate_count << "bitrate" << kbitrate;
                        bitrate_count = bitrate_count;
                        kbitrate = kbitrate/bitrate_count;

                        packet_info = " Bitrate (kbps) " + QString::number(kbitrate) +
                                  " Packets since last PCR " + QString::number(packets_since_pcr) +
                                  " bytes since last PCR "   + QString::number(bytes) +
                                  " PCR Ticks between PCR "  + QString::number(delta_pcr) +
                                  " ms between PCR "   + QString::number(ms_between_pcr) +
                                  " pcr = "            + QString::number(pcr.composite)  +
                                  " base pcr = "       + QString::number(pcr.base)       +
                                  " pcr remainder = "  + QString::number(pcr.remainder)  +
                                  " PID = "            + QString::number(pid)            +
                                  " from packet "      + QString::number(packets_read+1)        ;
                        info_list.append(packet_info);
                        ts_properties.pcr_period_ms = ms_between_pcr;
                        ts_properties.kbitrate = kbitrate;
                        ts_properties.info = info_list;

                    log( "Bitrate Found " + QString::number(kbitrate) + " kbps") ;

                    }
                    last_pcr_packet = packets_read;
                    last_pcr = pcr.composite;
                }
            }
        }
    return(ts_properties);
}
PCR TS_Info::getPCR(QByteArray packet)
{
    PCR pcr;
    pcr.base = 0;
    pcr.remainder = 0;
    pcr.composite = 0;
 // Get pcr from packet
    pcr.base =                    static_cast<quint8>(packet[6] );	     // base PCR bits 33..26
    pcr.base = (pcr.base << 8) +   static_cast<quint8>(packet[7] );         // base PCR bits 25..18
    pcr.base = (pcr.base << 8) +   static_cast<quint8>(packet[8] );         // base PCR bits 17..10
    pcr.base = (pcr.base << 8) +   static_cast<quint8>(packet[9] );         // base PCR bits  9.. 2
    pcr.base = (pcr.base << 1) +   (static_cast<quint8>(packet[10]) >>7 );    // base PCR bit   1  is in hi bit of "Padding Byte"  1000 0000

    pcr.remainder =                          static_cast<quint8>(packet[10]) & 0x01 ; // 9th ext bit in low bit of "Padding Byte"  0000 0001
    pcr.remainder = (pcr.remainder << 8) +   static_cast<quint8>(packet[11]) ;       // 8 ext bits in last PCR byte

// Calculate composite pcr from PCR Base and Extension
    pcr.composite = (300*pcr.base) + pcr.remainder;

    return pcr;
}

void TS_Info::log(QString logdata)
{
    emit status( (QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss:zzz ") + logdata + "\n") ) ;

}
