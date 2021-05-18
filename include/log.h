#ifndef LS_LOG_H
#define LS_LOG_H
void LogDebug(const char *fmt, ...);
void LogError(const char *fmt, ...);
void LogFatal(const char *fmt, ...);

// 初始化log输出文件以及创建log线程
int LogInit(const char *file);
void LogStopAndWait();

#endif