#include "ffmpeg.h"

FFmpeg::FFmpeg( QObject *parent) :
    QObject(parent)
{
    mInputPlayProcess = new QProcess(this);
    mFileInfoProcess = new QProcess(this);
    mTranscodingProcess = new QProcess(this);
    encoding = false;
    connect(mTranscodingProcess, SIGNAL(started()), this, SLOT(processStarted()));
    connect(mTranscodingProcess,SIGNAL(readyReadStandardOutput()),this,SLOT(readyReadStandardOutput()));
    connect(mTranscodingProcess, SIGNAL(finished(int)), this, SLOT(encodingFinished()));

    connect(mInputPlayProcess, SIGNAL(started()), this, SLOT(ffplay_processStarted()));
    connect(mInputPlayProcess,SIGNAL(readyReadStandardOutput()),this,SLOT(readyread_ffplay()));
    connect(mInputPlayProcess, SIGNAL(finished(int)), this, SLOT(playFinished()));

    connect(mFileInfoProcess, SIGNAL(started()), this, SLOT(analysis_started()));
    connect(mFileInfoProcess,SIGNAL(readyReadStandardOutput()),this,SLOT(analysis_stdout()));
    connect(mFileInfoProcess, SIGNAL(finished(int)), this, SLOT(analysis_finished()));
}

FFmpeg::~FFmpeg()
{
    kill();
}

void FFmpeg::close()
{

}

void FFmpeg::close_encoder()
{
    mTranscodingProcess->write("q");
    mTranscodingProcess->closeWriteChannel();
}

void FFmpeg::playFinished()
{
    log("play finished");
}

void FFmpeg::ffplay_processStarted()
{
    log("ffplay process started");
}

void FFmpeg::readyread_ffplay()
{
    //emit ffplay_stdout(mInputPlayProcess->readAllStandardOutput());
}

void FFmpeg::analysis_finished()
{
    log("analysis finished");
}

void FFmpeg::analysis_started()
{
    log("analysis started");
}

void FFmpeg::analysis_stdout()
{
    QString string;
    string = mFileInfoProcess->readAllStandardOutput();
    emit analysis_stdout_display(string);
    if(string.contains("format.bit_rate"))
    {
        QStringList stringlist;
        QString bitrate_line;
        stringlist = string.split("\n");
        stringlist = stringlist.filter("format.bit_rate");
        bitrate_line = stringlist.first();
        bitrate_line.chop(2);
        FileInfo_bitrate = bitrate_line.remove("format.bit_rate=\"", Qt::CaseInsensitive).toInt();
    }
}

void FFmpeg::kill_encoder()
{
    close_encoder();
    //mTranscodingProcess->kill();
    mTranscodingProcess->waitForFinished();
    //qDebug("transcode processes finished");
}

void FFmpeg::kill()
{
    mInputPlayProcess->kill();
    mInputPlayProcess->waitForFinished();
    kill_encoder();
}

void FFmpeg::processStarted()
{
    emit encode_started();
}

void FFmpeg::encode(QString inputfile,QString outputfile, bool capture,bool crossbar,int crossbar_pin,QString vdev,QString adev,int dialnorm)
{
    while(encoding)
    {
        QThread::msleep(10);
    }
    qDebug("encoding process starting");
    encoding = true;
    QString program;
    program = "./FFmpeg/bin/ffmpeg.exe";
    program = QFileInfo(program).absoluteFilePath();
    encode_fileName = outputfile;
    QStringList arguments;
    arguments.clear();
    //arguments << "-v" << "9" << "-loglevel" << "99" ;
    arguments  << "-re" << "-rtbufsize" << "100000k";

    if(capture)
    {
        arguments <<"-f" << "dshow" ;
        if(crossbar)
            arguments <<"-crossbar_video_input_pin_number" << QString::number(crossbar_pin);

        arguments << "-i" << ( "video=" + vdev + ":audio=" + adev );
    }
    else
        arguments << "-i" << inputfile;       // to read from a file

    arguments << "-f" << "mpegts"
    << "-muxrate" << "4000k"
    << "-mpegts_transport_stream_id" << "8471"
    << "-metadata" << "service_provider=\"K33EJ-D\""

    << "-vf" << "fps=29.97,scale=704x480"
    << "-vcodec" << "mpeg2video"
    << "-b:v" << "2000k"
    << "-pix_fmt" << "yuv420p"

    << "-acodec" <<"ac3"
    << "-af" << "pan=stereo|c0=c0|c1=c1"
    << "-ar" << "48000" << "-b:a" << "120k" << "-dialnorm" << QString::number(dialnorm)

    << "-mpegts_pmt_start_pid" <<  "0x40"
    << "-mpegts_start_pid" << "0x44"
    << "-metadata" << "service_name=\"Local\""
    << "-mpegts_service_id" << "1"
    << "-mpegts_original_network_id" << "7654"
    << "-tables_version" << "10"
    << "-threads" << "1"
    << outputfile;

    //qDebug()<< arguments;
    mTranscodingProcess->setProcessChannelMode(QProcess::MergedChannels);
    mTranscodingProcess->start(program, arguments);
}

int FFmpeg::file_info(QString inputfile)
{
    qDebug("file info");
    QString program;
    program = "./FFmpeg/bin/ffprobe.exe";
    program = QFileInfo(program).absoluteFilePath();
    QStringList arguments;
    arguments.clear();
    arguments   << "-v" << "error" << "-print_format" << "flat";
    arguments   << "-show_format" <<"-show_programs"<< inputfile;

   // arguments << "-v" << "9" ;//<< "-loglevel" << "99" ;
    //arguments << "-i" << inputfile       // to read from a file
    //          << "-af" << "volumedetect"
    //          << "-vn" << "-f" << "null" << "NUL";

    qDebug("problem line 1");
    mFileInfoProcess->setProcessChannelMode(QProcess::MergedChannels);
    qDebug("problem line 2");
    mFileInfoProcess->start(program, arguments);
    qDebug("problem line 3");
    mFileInfoProcess->waitForFinished();
    qDebug("here");
    return(FileInfo_bitrate);
}

void FFmpeg::readyReadStandardOutput()
{
    log_ffmpeg_stdout(mTranscodingProcess->readAllStandardOutput());
}

void FFmpeg::encodingFinished()
{
    encoding = false;
    if (QFile::exists(encode_fileName))
    {
        emit ffmpeg_finished( true );
        ffmpeg_stdout("Transcoding Status: Stopped!");
        log("Transcoding Status: Stopped!");
    }
    else
    {
        ffmpeg_stdout("Transcoding Status: Failed!");
        emit ffmpeg_finished( false );
        log("Transcoding Status: Failed!");
    }
}

void FFmpeg::ffplay(QString inputfile)
{
    QString program;
    program = "./FFmpeg/bin/ffplay.exe";
    program = QFileInfo(program).absoluteFilePath();
    QStringList arguments;
    //arguments << "video_size" << "172x120";

    arguments << inputfile;


    if(mInputPlayProcess->state()==0)
    {
        log("FFplay " + inputfile);
        mInputPlayProcess->setProcessChannelMode(QProcess::MergedChannels);
        mInputPlayProcess->start(program, arguments);
    }
}

void FFmpeg::log(QString logdata)
{
    emit ffmpeg_status( (QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss:zzz  ") + logdata + "\n") ) ;
}

void FFmpeg::log_ffmpeg_stdout(QString logdata)
{
    if(logdata!= "\r")
    emit ffmpeg_stdout( (QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss:zzz  ") + logdata + "\n") ) ;
}
