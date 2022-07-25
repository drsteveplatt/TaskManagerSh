//
// TaskManager Shell
//  An interactive shell task for TaskManager
//  Non-blocking

//  Implementation note:
//  	Used to use ArduinoSTL (https://github.com/mike-matera/ArduinoSTL)
//  	Switched to Arduino Vector (https://github.com/janelia-arduino/Vector) until 
//  	 the Matera library is fixed (incompatible new() issue)
//


#if !defined(__TASKMANAGERSH__)
#define __TASKMANAGERSH__

// Are we using ArduinoSSH or Array?
#define USING_ARDUINOSSH false

// The things we need
#if defined(ARDUINO_ARCH_ESP3266) || defined(ARDUINO_ARCH_ESP32)
#include <FS.h>
#include <SPIFFS.h>
#endif

#if USING_ARDUINOSSH
#include <ArduinoSTL.h>
#include <vector>
using namespace std;
#else
#include <Array.h>
#endif

// shell command tasks are in the range 208-239
#define READLINE_TASK 239
#define SHELL_TASK 238
#define READINTOPASTEBUFFER_TASK 237
//#define REBOOT_TASK 236
#if defined(ARDUINO_ARCH_ESP3266) || defined(ARDUINO_ARCH_ESP32)
#define ED_TASK 236
#endif
// end of shell command tasks

#if !USING_ARDUINOSSH
#define TMSH_MAX_PARAMS 10
#endif
// Note: to maintain compatibility with AVR systems, we need
// to keep Tmsh_param under 27 bytes.
// Instead, we pass around Tmsh_paramP things, pointers to Tmsh_param structs.
struct Tmsh_param {
  int Argc;
#if USING_ARDUINOSSH
  vector<String> Argv;
  Tmsh_param(int argc, vector<String> argv): Argc(argc), Argv(argv) {};
#else
  Array<String,TMSH_MAX_PARAMS> Argv;
  Tmsh_param(int argc, Array<String, TMSH_MAX_PARAMS> argv): Argc(argc), Argv(argv) {};
#endif
  Tmsh_param(): Argc(0) {}
};
typedef Tmsh_param* Tmsh_paramP;

// readline params and routines
// The parameter to READLINE_TASK
// It contains a single String*
// Constructors:  empty, and for a pre-existing String.
struct Tmsh_readlineParam {
  String* sp;
  Tmsh_readlineParam(String* s): sp(s) {}
  Tmsh_readlineParam() {}
};

void Tmsh_readlineTask();
#if USING_ARDUINOSSH
void Tmsh_readlineBufTokenize(String line, int& argc, vector<String>(& argv));
#else
void Tmsh_readlineBufTokenize(String line, int& argc, Array<String, TMSH_MAX_PARAMS>(& argv));
#endif

class TaskManagerSh {
	public:
		TaskManagerSh() {};
		~TaskManagerSh() {};

		void begin();
		bool addCommand(tm_taskId_t taskId, const char* taskName, void (*task)());
		bool addCommand(tm_taskId_t taskId, const String taskName, void (*task)()) {
			return addCommand(taskId, taskName.c_str(), task);
		};
};

extern TaskManagerSh TaskMgrSh;

#endif
