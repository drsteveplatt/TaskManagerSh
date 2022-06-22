//
// Shell processor
//
// Allows the user to do command line processing.
//
// The user writes subtasks and binds them to a name:
//    #include "readline.h"
//    #include "sh.h"
//    #define MYSUBTASK 200
//    void mysubtask() {
//      TM_BEGINSUB();
//      ...
//      TM_ENDSUB();
//    }
//    TaskMgr.add(SHELLTASK, shelltask);
//    TM_ADDSUBTASK(READLINETASK, readlinetask);
//    // repeat the next two lines for each command (Argv[0] value)
//    TM_ADDSUBTASK(MYSUBTASK, mysubtask);
//    shBindShCommand("somename", MYSUBTASK);
//
//    The subtask has access to globals int Argc and String Argv[SH_MAX_TOKENS].
//    There is no return value for the subtask
//

#include  <Streaming.h>
#include  <TaskManagerESPSub.h>
#include <vector>
using namespace std;

#include <FS.h>
#include <SPIFFS.h>

#include  <TaskManagerSh.h>
#include  "utils.h"

#define SH_MAXCOMMANDS  32
#define SH_MAXCOMMANDLEN  16
struct ShCommand {
  int taskId;
  char  cmd[SH_MAXCOMMANDLEN];
};

static ShCommand theCommands[SH_MAXCOMMANDS+1];
static int numCommands = 0;

static void shHelp() {
  int i;
  for(i=0; i<numCommands; i++)
    Serial.printf("  %s\n", theCommands[i].cmd);
}

//static bool shellBindShCommand(char* cmdName, int cmdTask) {
static void shellTask();
static void shellAddSubtasks();
void edTask();
void readIntoPasteBufferTask(); // from ed

void TaskManagerSh::begin() {
	TaskMgr.add(SHELL_TASK, shellTask);
	shellAddSubtasks();
}

bool TaskManagerSh::addCommand(tm_taskId_t cmdTask, const char* cmdName, void (*task)()) {
  if(numCommands==SH_MAXCOMMANDS) return false;
  if(strlen(cmdName)>SH_MAXCOMMANDLEN) return false;
  // else safe to add.
  theCommands[numCommands].taskId = cmdTask;
  strcpy(theCommands[numCommands].cmd, cmdName);
  numCommands++;

  TM_ADDSUBTASK(cmdTask, task);
  return true;
}

TaskManagerSh TaskMgrSh;

static void shellTask() {
  static int i;
  static String readlineBuf;
  static int Argc;
  static vector<String> Argv;
  static ShParam shParam(&Argc, &Argv);
  //static ReadlineParam rp(readlineBuf);
  static ReadlineParam rp(&readlineBuf);
  TM_BEGIN();
  Serial.print("cmd: ");
  TM_CALL_P(2, READLINE_TASK, rp);
  readlineBufTokenize(readlineBuf, Argc, Argv);
  if(Argc==0) TM_RETURN();	// no line to process

  // Search the user commands, and if none found, try the builtins
  // *** USER COMMANDS
  for(i=0; i<numCommands; i++) if(strcmp(theCommands[i].cmd, Argv[0].c_str())==0) break;
  if(i<numCommands) {
    TM_CALL_P(1, theCommands[i].taskId, shParam);
  } else if(Argv[0]=="appendTo") {            // *** APPENDTO
    int i;
    if(Argc<2) { Serial << "syntax: appendTo fn text text text...\n"; }
    else {
      for(i=2; i<Argc; i++) {
        appendTo(SPIFFS, Argv[1].c_str(), Argv[i].c_str());
        appendTo(SPIFFS, Argv[1].c_str(), "\n");
       }
    }
  } else if(Argv[0]=="cat") {                  // *** CAT
    if(Argc!=2) { Serial << "syntax: cat fn\n"; }
    else { cat(SPIFFS, Argv[1].c_str());  }
  } else if(Argv[0]=="echoTo") {
    if(Argc<2) { Serial << "syntax: echoTo fn text text text...\n"; }
    else {
      int i;
      rm(SPIFFS, Argv[1].c_str());
      echoTo(SPIFFS, Argv[1].c_str(),"");
      for(i=2; i<Argc; i++) {
        appendTo(SPIFFS, Argv[1].c_str(), Argv[i].c_str());
        appendTo(SPIFFS, Argv[1].c_str(), "\n");
      }
    }
  } else if(Argv[0]=="cp") {                  // *** CP
    if(Argc<3) { Serial << "Syntax: cp f f... fdest\n"; }
    else {
      cp(SPIFFS, Argv[1].c_str(), Argv[Argc-1].c_str());
      for(int i=1; i<Argc-1; i++) appendFile(SPIFFS, Argv[Argc-1].c_str(), Argv[i].c_str());
    }
  } else if(Argv[0]=="ed") {                  // *** ED
    if(Argc>2) Serial.println("Syntax: ed [filename]");
    else {
      TM_CALL_P(3, ED_TASK, shParam);
    }
  } else if(Argv[0]=="format") {              // *** FORMAT
    format(SPIFFS,"/");
  } else if(Argv[0]=="xget") {                 // *** GET
    if(Argc!=3) { Serial << "Syntax: get remotefn localfn\n"; }
    else {
      String remoteFn;
      remoteFn = (Argv[1][0]=='/' ? "" : "/") + Argv[1];
      Serial.printf("Fetching file [%s]\n", remoteFn.c_str());
      //getFromWeb(SPIFFS, hwInfo.host, remoteFn/*(*(shParam.Argv))[1]*/, Argv[2]);
    }
  } else if(Argv[0]=="help") {                // *** HELP
    shHelp();
  } else if(Argv[0]=="mv") {                  // *** MV
    if(Argc!=3) { Serial << "Syntax: mv fold fnew\n"; }
    else {
      mv(SPIFFS, Argv[1].c_str(), Argv[2].c_str());
    }
  } else if(Argv[0]=="ls") {                  // *** LS
    ls(SPIFFS, "/", 0);
  } else if(Argv[0]=="xput") {                 // *** PUT
    if(Argc!=3) { Serial.println("Syntax: put localfn remotefn"); }
    else {
      String remoteFile;
      remoteFile = (Argv[2][0]=='/'?"":"/") + Argv[2];
      Serial.printf("Putting [%s] to  remote file: [%s]\n", Argv[1].c_str(), remoteFile.c_str());
      //putToWeb(SPIFFS, hwInfo.host, Argv[1], remoteFile);
    }
  } else if(Argv[0]=="reboot") {              // *** REBOOT
    if(Argc!=1) { Serial.println("Syntax: reboot\n"); }
    else {
      Serial.println("Rebooting...");
      ESP.restart();
    }
  } else if(Argv[0]=="xreflash") {             // *** REFLASH
  	if(Argc==1) {
		// just reflash, so use appRoot + binFile
		//if(otaReflash(hwInfo.host, hwInfo.appRoot+hwInfo.binFile)) Serial.println("Image loaded successfully.");
		//else Serial.println("Image load failed.");

	} else if(Argc!=2) { Serial.println("Syntax: reflash fn"); }
    else {
      //if(otaReflash(hwInfo.host, Argv[1])) { Serial.println("Image loaded successfully."); }
      //else { Serial.println("Image load failed."); }
    }
  } else if(Argv[0]=="rm") {                  // *** RM
    if(Argc<2) { Serial << "Syntax: rm fil fil...\n"; }
    else {
      for(int i=1; i<Argc; i++) rm(SPIFFS, Argv[i].c_str());
    }
  } else Serial.println("Invalid command.");
  TM_END();
}

static void shellAddSubtasks() {
  // add the subtasks for core subtasks and builtin shell commands that aren't handled in shellTask
  TM_ADDSUBTASK(ED_TASK, edTask);
  TM_ADDSUBTASK(READLINE_TASK, readlineTask);
  TM_ADDSUBTASK(READINTOPASTEBUFFER_TASK, readIntoPasteBufferTask);
}

//
// readline subtask
// reads a single line from Serial
// saves in global string variable readlineBuf
// nonblocking
//
// Note: \ is an escape char, so " can be put in the string.
// Both the \ and the subsequent char will be left in the string.
//
void readlineTask() {
  TM_BEGINSUB_P(ReadlineParam, readlineParam);
  // **** start of nonblocking readline.
  // Read a line up to /r/n or /r or /n into String readlineParam.sp*
    char ch;
    bool done;
    bool lastWasCr;
    static String* readlineBuf;
    readlineBuf = readlineParam.sp;
    *readlineBuf = "";
    done = false;
    lastWasCr = false;
    while(!done) {
      //while(!Serial.available()) continue;
      while(!Serial.available()) { TM_YIELD(1); }
      ch = Serial.read();
      // Backspace processing for PuTTY
      if(ch==0x08) {
        if(readlineBuf->length()>0) {
          Serial << '\b' << ' ' << '\b';
          readlineBuf->remove(readlineBuf->length()-1);
        }
      } else {
        // CR processing for PuTTY
        if(ch=='\r') { ch='\n'; lastWasCr = true; }
        else if(ch=='\n' && lastWasCr) { lastWasCr=false; continue; } // crlf, skip the lf
        if(ch=='\n') { Serial << ch; lastWasCr = false; done = true; }
        else {
          Serial << ch;
          *readlineBuf += ch;
          lastWasCr = false;
        } // end if(ch=='\n') else
      } // end if(ch==0x08) else
    } // end while !done
  TM_ENDSUB();
}

void readlineBufTokenize(String line, int& argc, vector<String>& argv) {
  // tokenize readlineBuf into an argc/argv set, up to maxTokens supported.
  // Note that quoted strings are supported, as well as escape char "\" inside the quoted string.
  // Note that up to maxTokens tokens are read in; the rest are discarded unceremoniously
  // An open string at the end of the line will also be discarded.
  // There will be no \r or \n in the string.
  int curPos;
  int startPos, endPos;   // markers for a single token
  String tok;
  argc = 0;
  curPos = 0;
  //for(int i=0; i<maxTokens; i++) argv[i] = "";
  argv.clear();
  // now scan the string, separating on space or tab
  while(curPos<line.length()) {
    // grab a token
    startPos = curPos;
    // skip whitespace
    while(line[curPos]==' ' || line[curPos]=='\t') curPos++;
    // nothing left after the whitespace then done
    if(curPos==line.length()) break; // spaces were at the end
    // we have a token!  Process it.  Different processing depending on if a " or not
    if(line[curPos]=='"') {
      // double-quoted-string, go to closing '"', including escape-char '\' processing
      // '\' just passes the next char unchanged, no special \t \n etc processing (they become t, n, etc.)
      curPos++;
      startPos = curPos;
      while(curPos<line.length() && line[curPos]!='"') {
        // process the char.  if a '\', skip the '\' and process the next char
        if(line[curPos]=='\\') {
          // rip out the "\" and skip over the "
          line = line.substring(0, curPos) + line.substring(curPos+1);
          curPos++;
        } else if(curPos<line.length()) curPos++;   // account for '\' at end of line
      }
      // set up endPos
      if(curPos==line.length()) endPos = curPos; else { endPos = curPos-1; curPos++; }
    } else {
      // not a double-quoted-string, go to space or tab.  Yes, embedded doublequotes are part of the string.
      startPos = curPos;
      curPos++;
      while(curPos<line.length() && line[curPos]!=' ' && line[curPos]!='\t') curPos++;
      endPos = curPos-1;
    }
    // now, startPos is the first char in the string and endPos is the last char of the string
    argv.push_back(line.substring(startPos, endPos+1));
  }
  argc = argv.size();
}
