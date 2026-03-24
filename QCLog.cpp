#include "QCLog.h"

#include <QMutexLocker>
#include <QDir>
#include <QTime>
#include <QThread>
#include <QCoreApplication>

#define LOG_SUFFIX ".log"   //日志文件后缀

namespace qclog
{
    QtMessageHandler g_msgHndDef = nullptr; //默认的日志回调
    QFile g_fileLog;            //日志文件
    QMutex g_mtxFile;           //日志锁，防止打印/输出的日志混杂
    StLogCfg g_stLCfg;          //日志配置信息
    QString g_szLogFilePath;    //当前的日志文件路径
    QDate g_dtLogFile;          //当前的日志文件日期
    quint64 g_uFilePartNum = 0; //当前分片数，这个用全局缓存，以防止中途删除分片文件导致的序号前移，此处逻辑保持为顺序递增。于装载时初始化为0，正式序号从1开始。后面有依赖这个变量的类型的逻辑判断，请谨慎更换变量类型
    ELogLevel g_eLogLevel = ELogLevel::INFO;//日志等级

static void LogCallBack(QtMsgType, const QMessageLogContext&, const QString&);

//尝试删除历史日志
static void TryDeleteHistoryLog(){
    //是否进行日志文件有效期检查
    if(g_stLCfg.iFileVaildDay <= 0)
        return;
    QDir dirLog(g_stLCfg.szDirPath);
    //清除有效期外的文件
    //日志文件存在的情况下进行文件日期有效性检查
    const QStringList listFiles = dirLog.entryList(QDir::Files);
    for (const QString& szFindFileName : listFiles) {
        //如果文件不以.log结尾则略过
        if(false == szFindFileName.endsWith(LOG_SUFFIX))
            continue;
        //如果文件不以日志基础名称开头则略过
        if(false == g_stLCfg.szFileName.isEmpty() && false == szFindFileName.startsWith(g_stLCfg.szFileName))
            continue;
        const QFileInfo fileInfo(dirLog.absoluteFilePath(szFindFileName));
        // 获取文件的修改时间
        const QDateTime dtFileModifiedTime = fileInfo.lastModified();
        const QString tempName{ dtFileModifiedTime.toString("yyyy-MM-dd HH:mm::ss") };
        qDebug() << "file:" << szFindFileName << ", modified time:" << tempName;
        // 计算时间差
        qint64 iDaysDif = dtFileModifiedTime.date().daysTo(g_dtLogFile);
        if (iDaysDif > g_stLCfg.iFileVaildDay) { //日志文件超过有效期，并且不是最后一个文件，则删除该文件
            if (dirLog.remove(szFindFileName)) {
                qDebug() << "Deleted file:" << szFindFileName;
            }
            else {
                qWarning() << "Failed to delete file:" << szFindFileName;
            }
        }
    }
}

//基于已有的日期和分片号，尝试打开一个新的日志文件进行写入
static bool TryOpenNewFile(){
    static bool isFullPartNum = false;//当天的分片日志是否已达到过最大数
    bool isFileBeChanged = false;
    const QDate dtCur = QDate::currentDate();
    //文件打开了->日期没变->文件未达分片->不作为
    //文件打开了->日期没变->文件达分片->更换写入
    //文件打开了->日期变了->文件未达分片->更换写入
    //文件打开了->日期变了->文件达分片->更换写入
    //文件没打开->文件未达分片->更换写入
    //文件没打开->文件达分片->更换写入
    if(g_fileLog.isOpen()
            && g_dtLogFile == dtCur
            && g_stLCfg.iFileMaxByte > 0
            && g_fileLog.size() < g_stLCfg.iFileMaxByte)
        return isFileBeChanged;

    //日期更迭？
    if(g_dtLogFile != dtCur){
        g_dtLogFile = dtCur;
        g_uFilePartNum = 0;
        isFullPartNum = false;
        g_fileLog.close();
    }
    //获取路径前半部分:这里分片数可以继续增加，但日期一定要更新为最新
    const QString szFileBasePath = g_stLCfg.szDirPath + g_stLCfg.szFileName + g_dtLogFile.toString("yyyy-MM-dd");

    //文件没打开就直接打开最新日期下的文件先
    if(false == g_fileLog.isOpen()){
        g_szLogFilePath = szFileBasePath + LOG_SUFFIX;
        g_fileLog.setFileName(g_szLogFilePath);
        if (false == g_fileLog.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            qCritical() << "创建并打开日志文件失败:" << g_szLogFilePath << g_fileLog.errorString();
            return isFileBeChanged;
        }
        isFileBeChanged = true;
    }

    //对于已打开的文件，是否已可分片？
    if(g_stLCfg.iFileMaxByte <= 0 || g_fileLog.size() < g_stLCfg.iFileMaxByte)
        return isFileBeChanged;

    //开始分片
    while(true){
        if(g_uFilePartNum == UINT64_MAX){
            if(false == isFullPartNum){
                isFullPartNum = true;
                qWarning() << "FilePartNum has be max:" << UINT64_MAX;//分片功能失效，由于仍然能写物理文件，此处设置为警告级别
            }
            //不可继续分片，继续在最后一个分片上写文件
            break;
        }
        //递增序号，设置为下一个分片文件
        const QString szLogFilePathCur = QString("%1_%2%3").arg(szFileBasePath).arg(++g_uFilePartNum).arg(LOG_SUFFIX);
        QFile fileCur;
        fileCur.setFileName(szLogFilePathCur);
        //如果存在，并且文件已满则继续递增序号
        if(fileCur.exists() && fileCur.size() >= g_stLCfg.iFileMaxByte){
            continue;
        }
        //存在可用的分片文件路径
        g_szLogFilePath = szLogFilePathCur;
        //关闭文件，开始更换
        g_fileLog.close();
        g_fileLog.setFileName(szLogFilePathCur);
        if (false == g_fileLog.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            qCritical() << "创建并打开日志文件失败:" << szLogFilePathCur << g_fileLog.errorString();    //写日志到磁盘的功能完全失效，设置为错误级别
            break;
        }
        isFileBeChanged = true;
        break;
    }
    return isFileBeChanged;
}

void InstallLog(const QString &szFormat){
    //装载日志回调，并记录QT原有回调
    g_msgHndDef = qInstallMessageHandler(LogCallBack);
    g_stLCfg.szFormat = szFormat;
    g_stLCfg.iFileMaxByte = 0;
    g_stLCfg.iFileVaildDay = 0;
    g_stLCfg.szDirPath = "";
    g_stLCfg.szFileName = "";
}

bool InstallLogFile(const StLogCfg &stLogCfg)
{
    //装载日志回调，并记录QT原有回调
    g_msgHndDef = qInstallMessageHandler(LogCallBack);
    g_stLCfg = stLogCfg;
    //处理写入文件夹的路径
    if(g_stLCfg.szDirPath.isEmpty()){
        g_stLCfg.szDirPath = QCoreApplication::applicationDirPath() + "/log/";
    }else if(g_stLCfg.szDirPath.endsWith('\\') || g_stLCfg.szDirPath.endsWith('/')){
        g_stLCfg.szDirPath += "log/";
    }else{
        g_stLCfg.szDirPath += "/log/";
    }
    //分片数归0
    g_uFilePartNum = 0;

    //准备日志文件的输出
    QDir dirLog(g_stLCfg.szDirPath);
    if (false == dirLog.exists() && false == dirLog.mkpath(g_stLCfg.szDirPath)) {
        qCritical() << "没有权限创建日志文件夹:" << g_stLCfg.szDirPath;
        return false;
    }

    //准备基础的日志全路径
    g_dtLogFile = QDate::currentDate();
    g_szLogFilePath = g_stLCfg.szDirPath + g_stLCfg.szFileName  + g_dtLogFile.toString("yyyy-MM-dd") + LOG_SUFFIX ;

    TryDeleteHistoryLog();

    if (false == TryOpenNewFile())
        return false;

    qInfo() << "==========================================================================================================================================";
    qInfo() << "日志文件准备就绪，创建于:" << g_szLogFilePath;
    return true;
}


static void LogCallBack(QtMsgType msgType, const QMessageLogContext& msgContext, const QString& szMsg) {
    //给消息追加详细参数
    QString szColor;
    QString szLevel;
    switch (msgType)
    {
    case QtDebugMsg:
        if(g_eLogLevel < ELogLevel::DEBUG)
            return;
        szColor = "\033[33m";
        szLevel = "D";
        break;
    case QtWarningMsg:
        if(g_eLogLevel < ELogLevel::WARN)
            return;
        szColor = "\033[35m";
        szLevel = "W";
        break;
    case QtCriticalMsg:
        if(g_eLogLevel < ELogLevel::CRITICAL)
            return;
        szColor = "\033[31m";
        szLevel = "E";
        break;
    case QtFatalMsg:
        if(g_eLogLevel < ELogLevel::FATAL)
            return;
        szColor = "\033[31m";
        szLevel = "F";
        break;
    case QtInfoMsg:
        if(g_eLogLevel < ELogLevel::INFO)
            return;
        szLevel = "I";
        break;
    default:
        szColor = "\033[36m";
        szLevel = "U";
         break;
    }


    QString szFunc = QString(msgContext.function);
    szFunc = szFunc.left(szFunc.indexOf('('));
    const QDate dtCur = QDate::currentDate();
    const QString szTime = dtCur.toString("yyyy-MM-dd") + " " + QTime::currentTime().toString("hh:mm:ss.zzz");

    QString szNewMessage = g_stLCfg.szFormat;
    szNewMessage.replace("%threadId", QString::number(reinterpret_cast<qint64>(QThread::currentThreadId())));
    szNewMessage.replace("%datetime", szTime);
    szNewMessage.replace("%pid", QString::number(QCoreApplication::applicationPid()));
    szNewMessage.replace("%level", szLevel);
    szNewMessage.replace("%file", msgContext.file ? QFileInfo(msgContext.file).fileName() : "unknown");
    szNewMessage.replace("%line", QString::number(msgContext.line));
    szNewMessage.replace("%function", szFunc);
    szNewMessage.replace("%message", szMsg);

    g_mtxFile.lock();
    while (g_fileLog.isOpen()) {
        //如果日期变更，那么写入前需要换个日志写
        if(g_dtLogFile != dtCur && false == TryOpenNewFile() && !g_fileLog.isOpen()){
            //文件切换并打开失败，不写入
            break;
        }
        //写入内容
        g_fileLog.write((szNewMessage + "\n").toLocal8Bit());
        g_fileLog.flush();

        //不需要分片就直接走
        if(g_stLCfg.iFileMaxByte <= 0){
            break;
        }
        //没达到分片大小就直接走
        if(g_fileLog.size() < g_stLCfg.iFileMaxByte){
            break;
        }
        //下一个分片文件
        TryOpenNewFile();
        break;
    }
    g_mtxFile.unlock();
    //输出到控制台
    szNewMessage = szColor + szNewMessage + "\033[0m";
    g_msgHndDef(msgType, msgContext, szNewMessage);
}

void UnInstallLog()
{
    if(g_msgHndDef)
        qInstallMessageHandler(g_msgHndDef);
    g_mtxFile.lock();
    if (g_fileLog.isOpen())
        g_fileLog.close();
    g_mtxFile.unlock();
}

void SetLogLevel(ELogLevel eLogLevel)
{
    if(eLogLevel > ELogLevel::MAX || eLogLevel < ELogLevel::MIN){
        qCritical() << "给定的日志等级不在范围内，设置失败";
        return;
    }
    g_eLogLevel = eLogLevel;
    qDebug() << "日志等级被设置为:" << (int)g_eLogLevel;
}

}//namespace qlog
