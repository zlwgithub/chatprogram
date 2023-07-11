#pragma once
#include "lockqueue_qqchat.h"
#include <string>

#define LOGINFO(logmsgFormat, ...)                                      \
    do                                                                  \
    {                                                                   \
        Log &logger = Log::GetInstance();                               \
        logger.setLogtype(ERROR);                                       \
        char buf[1024] = "[INFO]";                                      \
        snprintf(buf + strlen(buf), 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.Logput(buf);                                             \
    } while (0)

#define LOGERROR(logmsgFormat, ...)                                     \
    do                                                                  \
    {                                                                   \
        Log &logger = Log::GetInstance();                               \
        logger.setLogtype(ERROR);                                       \
        char buf[1024] = "[ERROR]";                                     \
        snprintf(buf + strlen(buf), 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.Logput(buf);                                             \
    } while (0)

#define LOGFATAL(logmsgFormat, ...)                                     \
    do                                                                  \
    {                                                                   \
        Log &logger = Log::GetInstance();                               \
        logger.setLogtype(FATAL);                                       \
        char buf[1024] = "[FATAL]";                                     \
        snprintf(buf + strlen(buf), 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.Logput(buf);                                             \
        exit(EXIT_FAILURE);                                             \
    } while (0)

#ifdef QQCHATDEBUG
#define LOGDEBUG(logmsgFormat, ...)                                     \
    do                                                                  \
    {                                                                   \
        Log &logger = Log::GetInstance();                               \
        logger.setLogtype(DEBUG);                                       \
        char buf[1024] = "[DEBUG]";                                     \
        snprintf(buf + strlen(buf), 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.Logput(buf);                                             \
    } while (0)
#else
#define LOGDEBUG(logmsgFormat, ...)
#endif

// 定义日志的级别  INFO  ERROR  FATAL  DEBUG
enum Logtype
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // core信息
    DEBUG, // 调试信息
};

// Mprpc框架提供的日志系统
class Log
{
public:
    // 获取日志的单例
    static Log &GetInstance();
    //  设置日志级别
    void setLogtype(Logtype level);
    // 写日志到lockqueue缓冲区中
    void Logput(std::string msg);

private:
    Logtype m_Logtype;              // 记录日志级别
    LockQueue_qqchat<std::string> m_lockQue; // 日志缓存队列

    Log();
    Log(const Log &) = delete;
    Log(Log &&) = delete;
};
