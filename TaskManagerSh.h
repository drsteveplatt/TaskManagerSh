//
// sh shell processor header
//

#if !defined(__SH__)
#define __SH__

#define READLINE_TASK 194
#define SHELL_TASK 195
// shell command tasks
#if false
#define CAT_TASK 196
#define LS_TASK 197
#define ECHO_TASK 198
#define FORMAT_TASK 199
#define APPEND_TASK 200
#define CP_TASK 201
#define RM_TASK 202
#define MV_TASK 203
#endif
#define GET_TASK 204
#define PUT_TASK 205
#define ED_TASK 206
#define READINTOPASTEBUFFER_TASK 207
#define REFLASH_TASK 208
#define REBOOT_TASK 209
// end of shell command tasks

struct ShParam{
  int* Argc;
  vector<String>* Argv;
  ShParam(): Argc(NULL), Argv(NULL) {}
  ShParam(int* argc, vector<String>* argv): Argc(argc), Argv(argv) {};
};

//void shellTask();
//void shellHelp();
//bool shellBindShCommand(char* cmdName, int cmdTask);
//void shellAddSubtasks();

// Editor task
//void edTask();
//void readIntoPasteBufferTask(); // from ed

// readline params and routines
// The parameter to READLINE_TASK
// It contains a single String*
// Constructors:  empty, and for a pre-existing String.
struct ReadlineParam {
  String* sp;
  ReadlineParam(String* s): sp(s) {}
  ReadlineParam() {}
};

void readlineTask();
void readlineBufTokenize(String line, int& argc, vector<String>(& argv));

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
