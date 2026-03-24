这是一个依赖QT原生日志模块的日志分片库，既可以编译他产生库文件以引入程序库中，也可以单独提取走项目中的的.h和.cpp这两个文件直接塞进项目里使用。  

支持功能有:  
1.自动按日志大小进行文件分片  
2.根据日志修改日期，对过期日志文件进行自动删除  
3.支持日志跨天写入  
4.编辑日志格式字符串，选择性地加入进程号，函数行号等详细信息  
5.随时可变的日志等级过滤输出  
  
使用例子  
#include <QCoreApplication>  
#include "QCLog.h"  
int main(int argc, char *argv[])  
{  
    QCoreApplication a(argc, argv);  
    qclog::StLogCfg stLCfg;  
    stLCfg.iFileMaxByte = 200;  
    stLCfg.iFileVaildDay = 1;  
    stLCfg.szDirPath = "";  
    stLCfg.szFileName = "ra_";  
    stLCfg.szFormat = "[%pid][%datetime][%threadId][%level][%file:%line, %function]\t%message";  
    //使用日志写入  
    qclog::InstallLogFile(stLCfg);  
    //使用格式化输出  
    //qclog::InstallLog("[%pid][%datetime][%threadId][%level][%file:%line, %function]\t%message");  
    for(int i=0;i<20;++i){  
        qInfo() << "hi";  
        qInfo() << "good";  
        qCritical() << "bad";  
        qWarning() << "sad";  
        _sleep(5000);  
    }  
    qclog::UnInstallLog();  
    qInfo() << "UnInstallLog";  
    return a.exec();  
}  

可以看出，引入头文件后，用qclog::InstallLogFile和qclog::UnInstallLog两个方法即可覆盖日志使用场景。  

在程序的其他运行时刻，也可以随时使用以下方法对日志消息进行等级过滤  
qclog::SetLogLevel(qclog::ELogLevel::CRITICAL);  
这个设置将会表明任何低于严重等级的消息都会被过滤。  
