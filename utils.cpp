//
// Various filesystem routines
//

#include <arduino.h>

#include <FS.h>
#include <SPIFFS.h>
#include <TaskManagerESPSub.h>

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

void getFromWeb(fs::FS &fs, String ip, const String remoteFn, const String localFn) {
  // reads remoteFn from the web server and puts in localFn
  // Assumes we've used TaskMgr.beginRadio(nodeId, ssid, pw) so
  // we have already established a connection to the WiFi network.
  static WiFiClient client;
  static bool connected;
  static int i;
  //static char line[255];
  //
  static int contentLength;
  static int httpReply;
  static String s;
  static char c;
  static String errMsg;
  static bool done;
  //
  File file;
  //TM_BEGINSUB();
  // ** Connect to web server
  i = 0;
  while(!(connected = client.connect(ip.c_str(), 80)) && i<20) {
    i++;
    //TM_YIELDDELAY(1,500);
    delay(250);
  }
  if(!connected) {
    // fail, no server connect
    Serial << "Connect failure to " << ip << ".\n";
    return;
  }
  // ** Send a GET
  client.printf("GET %s HTTP/1.1\n", remoteFn.c_str());
  client.printf("Host: %s\n", ip.c_str());
  client.printf("Connection: close\n\n");

  // ** Get the return code / process the header
  contentLength = 0;
  httpReply = 0;
  //TM_YIELDDELAY(2,50);    // give it a wee bit to respond
  delay(50);
  done = false;
  s = "";

  while(client.available() && !done) { // process header
    // header is a bunch of lines terminated with \r\n, ends with a blank line (just \r\n).
    // We're interested in the HTTP/ line and the Content-Length: line
    c = client.read();
    if(c=='\r') { // ignore, next should be a \n 
    } else if(c=='\n') { // process the line
      if(s.length()==0) { done = true; } 
      else if(s.startsWith("HTTP/")) {
        // parse out return code
        sscanf(s.c_str(), "%*s%d",&httpReply);
        errMsg = s;  // save the first line as the default message
      } else if(s.startsWith("Content-Length: ")) {
        sscanf(s.c_str(), "Content-Length: %d",&contentLength);
      } else { // something else, ignore the line
      }
      s = "";
    } else { // add the char to the current line
      s += c;
    }
  } // end of process header
  //Serial << "*** End of header\n";
  if(httpReply!=200) {
    client.stop();
    return;  // all done, it failed...                   
  }
  // ** Read and save the file

  file = fs.open(addSlash(localFn.c_str()).c_str(), FILE_WRITE);
  s = "";
  while(client.available()) {
    file.write(client.read());
  }
  file.close();
  client.stop();

  //TM_ENDSUB();
}

void putToWeb(fs::FS &fs, String ip, const String localFn, const String remoteFn) {
  // reads localFn from the local filesystem and puts in remoteFn on web server
  // Assumes we've used TaskMgr.beginRadio(nodeId, ssid, pw) so
  // we have already established a connection to the WiFi network.
  static WiFiClient client;
  static bool connected;
  static int i;
  static String payload;
  
  //TM_BEGINSUB();
  i=0;
  while(!(connected = client.connect(ip.c_str(), 80)) && i<20) {
    i++;
    //TM_YIELDDELAY(1,500);
    delay(250);
  }
  if(!connected) {
    // fail, no server connect
    Serial << "Connect failure to " << ip << ".\n";
    client.stop();
    return;
  }

  // ** Send a POST
#define SEPARATOR "00000--ESP32DATA--99999"
  client.printf("POST /php/updateFile.php HTTP/1.1\n");
  client.printf("Host: %s\n", ip.c_str());
  client.printf("Content-Type: multipart/form-data; boundary=\"%s\"\n", SEPARATOR);
  client.printf("Connection: close\n");
  // build the payload
  payload = String("--") + SEPARATOR + "\n"
    + "Content-Disposition: form-data; name=\"doIt\"\n\ndoIt\n--" + SEPARATOR + "\n"
    + "Content-Disposition: form-data; name=\"filename\"\n\n" + remoteFn + "\n--" + SEPARATOR + "\n";
  // add the file to the payload
  { 
    File oldFile = SPIFFS.open(addSlash(localFn.c_str()).c_str(), FILE_READ);
    if(!oldFile || oldFile.isDirectory()) { oldFile.close();  }
    else {
      payload += "Content-Disposition: form-data; name=\"contents\"\n\n";
      while(oldFile.available()) payload += char(oldFile.read());
      oldFile.close();      
      payload += String("\n--") + SEPARATOR + "--\n";
    }
  }
  // end of payload, send the payload length and end the header
  client.printf("Content-length: %d\n\n",payload.length());
  // send the payload
  client.println(payload.c_str());

  // ignore the response, just exit cleanly
  client.stop();
  //TM_ENDSUB();
}

bool otaReflash(String ip, String remoteFn) {
  // Loads the file fn from the web server at ip
  // Overwrites the current image
  // At the next boot, the new image will be booted
  // Returns true if the image was loaded successfully, false otherwise
  // (If false, then a reboot will reload the current image

  // Code pretty much copied from getFromWeb().  Eventually, we'll merge them as much as possible.
  
  // Reads remoteFn from the web server and saves as new bootable image
  // Assumes we've used TaskMgr.beginRadio(nodeId, ssid, pw) so
  // we have already established a connection to the WiFi network.
  static WiFiClient client;
  static bool connected;
  static int i;

  static long int contentLength, written;
  static int httpReply;
  static bool canBegin;

  static String line;
  //TM_BEGINSUB();
  // ** Connect to web server
  i = 0;
  while(!(connected = client.connect(ip.c_str(), 80)) && i<20) {
    i++;
    delay(250);
  }
  if(!connected) {
    // fail, no server connect
    Serial << "Connect failure to " << ip << ".\n";
    return false;
  }
  // ** Send a GET
  client.printf("GET %s HTTP/1.1\n", remoteFn.c_str());
  client.printf("Host: %s\n", ip.c_str());
  client.printf("Connection: close\n\n");

  // ** Get the return code / process the header
  contentLength = 0;
  httpReply = 0;
  //TM_YIELDDELAY(2,50);    // give it a wee bit to respond
  delay(50);

  while(client.available()) { // process header
    // header is a bunch of lines terminated with \r\n, ends with a blank line (just \r\n).
    // We're interested in the HTTP/ line and the Content-Length: line
    line = client.readStringUntil('\n');
    line.trim();
    if(line.length()==0) { break; } // blank line -> end of header

    if(line.startsWith("HTTP/")) {
      sscanf(line.c_str(), "%*s%d", &httpReply);  // get return code;
    } else if(line.startsWith("Content-Length: ")) {
      sscanf(line.c_str(), "Content-Length: %ld", &contentLength);
    } else {
      // ignore the line
    }
  }
  // done with header, check reply and see if we are going to run
  if(httpReply!=200) {
    client.stop();
    return false;  // all done, it failed...                   
  }
  
  // ** Read and save the file
  canBegin = Update.begin(contentLength);
  written = Update.writeStream(client);
  if(Update.end() && written==contentLength) {
    client.stop();
    return true;
  } else {
    client.flush();
    client.stop();
    return false;
  }

  //TM_ENDSUB();
}
