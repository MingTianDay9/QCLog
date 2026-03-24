#ifndef QCLOG_H
#define QCLOG_H

#include <QtCore/qglobal.h>
#include <QDebug>

#if defined(QCLOG_LIBRARY)
#  define QCLOG_EXPORT Q_DECL_EXPORT
#else
#  define QCLOG_EXPORT Q_DECL_IMPORT
#endif

namespace qclog
{
//日志配置信息
struct StLogCfg{
    QString szDirPath;      //日志文件创建在哪个文件夹?为空则默认当前
    QString szFileName;     //日志名称，如果没有将产生纯时间命名的日志，否则则是***2026-03-09.log这种格式
    QString szFormat;       //日志打印格式字符串[%pid][%datetime][%threadId][%level][%file:%line, %function]\t%message
    qint64 iFileMaxByte;    //日志文件最大值字节。<=0则代表不进行分片，否则则将进行日志分片记录
    int iFileVaildDay;      //日志文件保存几天。<=0代表不删日志，否则当修改日期超过特定天数后进行日志删除
};

//日志等级
enum class ELogLevel{
    FATAL = 0,
    CRITICAL,
    WARN,
    INFO,
    DEBUG,
    MIN = FATAL,
    MAX = DEBUG,
};

///
/// \brief 日志装载(无文件输出)
/// \param szFormat 打印格式字符串[%pid][%datetime][%threadId][%level][%file:%line, %function]\t%message
///
void QCLOG_EXPORT InstallLog(const QString &szFormat);

///
/// \brief 日志装载
/// \param stLogCfg 日志配置
///
bool QCLOG_EXPORT InstallLogFile(const StLogCfg &stLogCfg);

///
/// \brief 日志卸载，并关闭文件保存
///
void QCLOG_EXPORT UnInstallLog();

///
/// \brief 设置日志等级
/// \param eLogLevel 日志等级
///
void QCLOG_EXPORT SetLogLevel(ELogLevel eLogLevel);


}//namespace qclog

#endif // QCLOG_H
