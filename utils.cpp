//
// Various filesystem routines
//

#include <arduino.h>

#include <FS.h>
#include <SPIFFS.h>
#include <TaskManagerSub.h>

#include <Update.h>
#include <HTTPClient.h>

#include <Streaming.h>
#include "utils.h"

static char space32[] = "                                ";
static char* spaces(int n) {
  if(n<0) n=0; else if(n>32) n=32;
  return &(space32[32-n]);
}

String addSlash(const char* path) {
  if(path[0]=='/') return String(path);
  else return String("/")+path;
}

void ls(fs::FS &fs, const char* dirName, int levels) {
  Serial.printf("%s%s",spaces(levels*2), dirName);
  // Open it as a dir and print a line for it
  File root = fs.open(dirName);
  if(!root) return;
  Serial.print(root.isDirectory()?" (DIR)\n":" (not DIR)\n");
  // Now go through the files
  File file = root.openNextFile();
  while(file) {
    if(file.isDirectory()) {
      ls(fs, file.name(), levels+1);
    } else {
      Serial.printf("%s%s  %d\n", spaces(levels*2), file.name(), file.size());
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}
void cat(fs::FS &fs, const char* path) {
  File file = fs.open(addSlash(path).c_str(), FILE_READ);
  if(!file || file.isDirectory()) return;
  while(file.available()) Serial.write(file.read());
  file.close();
}
void echoTo(fs::FS &fs, const char* path, const char* content) {
  File file = fs.open(addSlash(path).c_str(), FILE_WRITE);
  if(!file || file.isDirectory()) return;
  file.print(content);
  file.close();
}
void appendTo(fs::FS &fs, const char* path, const char* content) {
  // Append a string to a file
  File file = fs.open(addSlash(path).c_str(), FILE_APPEND);
  if(!file || file.isDirectory()) return;
  file.print(content);
  file.close();
}
void mv(fs::FS &fs, const char* old, const char* newf) {
  fs.rename(addSlash(old).c_str(), addSlash(newf).c_str());
}
void rm(fs::FS &fs, const char* path) {
  fs.remove(addSlash(path).c_str());
}
void cp(fs::FS &fs, const char* old, const char* newf) {
  File oldFile = fs.open(addSlash(old).c_str(), FILE_READ);
  File newFile = fs.open(addSlash(newf).c_str());
  if(!old || oldFile.isDirectory() || !newFile || newFile.isDirectory()) { oldFile.close(); newFile.close(); return; }
  while(oldFile.available()) newFile.write(oldFile.read());
  oldFile.close();
  newFile.close();
}
void appendFile(fs::FS &fs, const char* dest, const char* src) {
  // append src file to dest file
  File destFile = fs.open(addSlash(dest).c_str(), FILE_APPEND);
  File srcFile = fs.open(addSlash(src).c_str());
  if(!dest || destFile.isDirectory() || !src || srcFile.isDirectory()) { srcFile.close(); destFile.close(); return; }
  while(srcFile.available()) destFile.write(srcFile.read());
  srcFile.close();
  destFile.close();
}
void format(fs::FS &fs, const char* dirName) {
  // just rm everything on fs
  File root = fs.open(dirName);
  if(!root) return;
  //Serial.print(root.isDirectory()?" (DIR)\n":" (not DIR)\n");
  // Now go through the files
  File file = root.openNextFile();
  while(file) {
    if(file.isDirectory()) {
      format(fs, file.name());
      rm(fs, file.name());
    } else {
      rm(fs, file.name());
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

