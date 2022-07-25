//
// declarations for utils
//

#if !defined(__TASKMANAGER_UTILSDEFINED__)
#define __TASKMANAGER_UTILSDEFINED__

#if false
#define NODEID 200
#define WEBSSID "sculpturama"
#define PW "$0$0flashy"
#define WEBIP "192.168.200.2"
#define WEBPATH "/data"
#endif

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
void ls(fs::FS &fs, const char* dirName, int levels);
void cat(fs::FS &fs, const char* path);
void echoTo(fs::FS &fs, const char* path, const char* content);
void appendTo(fs::FS &fs, const char* path, const char* content);
void mv(fs::FS &fs, const char* old, const char* newf);
void rm(fs::FS &fs, const char* path);
void cp(fs::FS &fs, const char* old, const char* newf);
void format(fs::FS &fs, const char* dirName);
void appendFile(fs::FS &fs, const char* old, const char* newf);
void getFromWeb(fs::FS &fs, const String IP, const String remoteFn, const String localFn);
void putToWeb(fs::FS &fs, const String IP, const String localFn, const String remoteFn);
bool otaReflash(String ip, String fn);
#endif // ESP32 arch
#endif // __TASKMANAGER_UTILSDEFINED__ defined
