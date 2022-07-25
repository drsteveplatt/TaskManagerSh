//
// Shell processor
//
// Allows the user to do command line processing within TaskManager
//	Reads/writes from Serial
//	Nonblocking
//
// The user writes subtasks and binds them to a name:
//    #include <TaskManager.h>
//	  #include <TaskManagerSh.h>

//    #define MYSUBTASK 200
//    void myCommand() {
//      TM_BEGINSUB();
//      ...
//      TM_ENDSUB();
//    }
//	  TaskMgr.begin();
//	  TaskMgrSh.begin();
//    TaskMgrSh.addCommand(MYSUBTASK, "cmdname", myCommand);
//
//	  Note that a subtask may also get all of its params from the command line
//		by using TM_BEGINSUB_P(Tmsh_paramP, shparam) instead of TM_BEGINSUB()
//		and then using shparam->Argc and shparam->Argv[] to access the params.
//
//	  Tested on ESP32 and Mega2560.  We do not recommend using 328-based systems
//		due to memory limitations.
//	  
//

//#include  <Streaming.h>
#include <Arduino.h>
#include  <TaskManagerSub.h>

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#include <FS.h>
#include <SPIFFS.h>
#endif

#include  <TaskManagerSh.h>
#include  "utils.h"

#if defined(ARDUINO_ARCH_AVR)
#define SH_MAXCOMMANDS 16
#define SH_MAXCOMMANDLEN 12
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#define SH_MAXCOMMANDS  32
#define SH_MAXCOMMANDLEN  16
#endif
struct ShCommand {
  int taskId;
  char cmd[SH_MAXCOMMANDLEN+1];
};

static ShCommand theCommands[SH_MAXCOMMANDS+1];
static int numCommands = 0;

static void shHelp() {
  int i;
  for(i=0; i<numCommands; i++) {
    Serial.print("  "); Serial.println(theCommands[i].cmd);
  }
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  Serial.print("  appendTo fn text text text...\n  cat fn\n");
  Serial.print("  echoTo fn text text text...\n  cp f f... fdest\n");
  Serial.print("  ed [filename]\n  mv fold fnew\n");
  Serial.print("  format\n  reboot\n");
  Serial.print("  help\n  mv fold fnew\n");
  Serial.print("  ls\n  rm fil fil...\n");
  Serial.print("  get remotefn localfn\n  put localfn remotefn\n");
  Serial.print("  reflash fn\n");
#endif
  Serial.print("  reboot\n");
}

//static bool shellBindShCommand(char* cmdName, int cmdTask) {
static void shellTask();
static void shellAddSubtasks();
void Tmsh_edTask();
void Tmsh_readIntoPasteBufferTask(); // from ed

void TaskManagerSh::begin() {
	// Format SPIFFS file system if needed; open SPIFFS filesystem
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  if(!SPIFFS.begin(false)) {
    Serial.print("Need to format, formatting.\n");
    SPIFFS.format();
    if(!SPIFFS.begin(false)) {
      Serial.print("Format failed, exiting.\n");
      return;
    }
    Serial.print("...format succeeded.\n");
  }
#endif  

  // add the shell task and all of its callable subtask
  TaskMgr.add(SHELL_TASK, shellTask);
  shellAddSubtasks();
}

bool TaskManagerSh::addCommand(tm_taskId_t cmdTask, const char* cmdName, void (*task)()) {
  if(numCommands==SH_MAXCOMMANDS) return false;
  if(strlen(cmdName)>SH_MAXCOMMANDLEN) return false;
  // else safe to add.
  theCommands[numCommands].taskId = cmdTask;
  strcpy(&(theCommands[numCommands].cmd[0]), cmdName);
  numCommands++;

  TM_ADDSUBTASK(cmdTask, task);
  return true;
}

TaskManagerSh TaskMgrSh;

static void shellTask() {
  static int i;
  static String readlineBuf;
  static int Argc;
#if USING_ARDUINOSSH
  static vector<String> Argv;
#else
  static Array<String, TMSH_MAX_PARAMS> Argv;
#endif
  static Tmsh_param shParam;
  static Tmsh_paramP shParamP;
  //static ReadlineParam rp(readlineBuf);
  static Tmsh_readlineParam rp(&readlineBuf);
  shParamP = &shParam;
  TM_BEGIN();
  Serial.print("cmd: ");
  TM_CALL_P(2, READLINE_TASK, rp);
  Serial.println(readlineBuf);
  Tmsh_readlineBufTokenize(readlineBuf, Argc, Argv);
  if(Argc==0) { TM_RETURN(); }	// no line to process

  // Search the user commands, and if none found, try the builtins
  // *** USER COMMANDS
  for(i=0; i<numCommands; i++) if(strcmp(theCommands[i].cmd, Argv[0].c_str())==0) break;
  if(i<numCommands) {
	//static ShParam shParam(Argc, Argv);
	shParam.Argc = Argc; shParam.Argv = Argv;
    TM_CALL_P(1, theCommands[i].taskId, shParamP);
  } else if(Argv[0]=="reboot") {              // *** REBOOT
    if(Argc!=1) { Serial.print("Syntax: reboot\n"); }
    else {
      Serial.println("Rebooting...");
	  Serial.flush();
#if defined(ARDUINO_ARCH_AVR)
	  asm volatile (" jmp 0");
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
      ESP.restart();
#endif
	}
  } else if(Argv[0]=="help") {                // *** HELP
    shHelp();
  } 
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  else if(Argv[0]=="appendTo") {            // *** APPENDTO
    int i;
    if(Argc<2) { Serial.print("syntax: appendTo fn text text text...\n"); }
    else {
      for(i=2; i<Argc; i++) {
        appendTo(SPIFFS, Argv[1].c_str(), Argv[i].c_str());
        appendTo(SPIFFS, Argv[1].c_str(), "\n");
       }
    }
  } else if(Argv[0]=="cat") {                  // *** CAT
    if(Argc!=2) { Serial.print("syntax: cat fn\n"); }
    else { cat(SPIFFS, Argv[1].c_str());  }
  } else if(Argv[0]=="echoTo") {
    if(Argc<2) { Serial.print("syntax: echoTo fn text text text...\n"); }
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
    if(Argc<3) { Serial.print("Syntax: cp f f... fdest\n"); }
    else {
      cp(SPIFFS, Argv[1].c_str(), Argv[Argc-1].c_str());
      for(int i=1; i<Argc-1; i++) appendFile(SPIFFS, Argv[Argc-1].c_str(), Argv[i].c_str());
    }
  } else if(Argv[0]=="ed") {                  // *** ED
    if(Argc>2) { Serial.print("Syntax: ed [filename]\n"); }
    else {
	  //static ShParam shParam(Argc, Argv);
	  shParam.Argc = Argc;  shParam.Argv = Argv;
      TM_CALL_P(3, ED_TASK, shParamP);
    }
  } else if(Argv[0]=="format") {              // *** FORMAT
    format(SPIFFS,"/");
  } else if(Argv[0]=="mv") {                  // *** MV
    if(Argc!=3) { Serial.print("Syntax: mv fold fnew\n"); }
    else {
      mv(SPIFFS, Argv[1].c_str(), Argv[2].c_str());
    }
  } else if(Argv[0]=="ls") {                  // *** LS
    ls(SPIFFS, "/", 0);
  } else if(Argv[0]=="rm") {                  // *** RM
    if(Argc<2) { Serial.print("Syntax: rm fil fil...\n"); }
    else {
      for(int i=1; i<Argc; i++) rm(SPIFFS, Argv[i].c_str());
    }
  } else if(Argv[0]=="xget") {                 // *** GET
	if(Argc!=3) { Serial.print("Syntax: get remotefn localfn\n"); }
    else {
      String remoteFn;
      remoteFn = (Argv[1][0]=='/' ? "" : "/") + Argv[1];
      Serial.printf("Fetching file [%s]\n", remoteFn.c_str());
      //getFromWeb(SPIFFS, hwInfo.host, remoteFn/*(*(shParam.Argv))[1]*/, Argv[2]);
    }
  } else if(Argv[0]=="xput") {                 // *** PUT
	if(Argc!=3) { Serial.println("Syntax: put localfn remotefn"); }
    else {
      String remoteFile;
      remoteFile = (Argv[2][0]=='/'?"":"/") + Argv[2];
      Serial.printf("Putting [%s] to  remote file: [%s]\n", Argv[1].c_str(), remoteFile.c_str());
      //putToWeb(SPIFFS, hwInfo.host, Argv[1], remoteFile);
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
  } 
#endif // defined (ESP architecture)
  else { Serial.println("Invalid command."); }
  TM_END();
}

static void shellAddSubtasks() {
  // add the subtasks for core subtasks and builtin shell commands that aren't handled in shellTask
  TM_ADDSUBTASK(READLINE_TASK, Tmsh_readlineTask);
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  TM_ADDSUBTASK(ED_TASK, Tmsh_edTask);
  TM_ADDSUBTASK(READINTOPASTEBUFFER_TASK, Tmsh_readIntoPasteBufferTask);
#endif
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
void Tmsh_readlineTask() {
  TM_BEGINSUB_P(Tmsh_readlineParam, readlineParam);
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

#if USING_ARDUINOSSH
void Tmsh_readlineBufTokenize(String line, int& argc, vector<String>& argv) {
#else
void Tmsh_readlineBufTokenize(String line, int& argc, Array<String, TMSH_MAX_PARAMS>& argv) {
#endif
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
