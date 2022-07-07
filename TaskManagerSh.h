//
// sh shell processor header
//

#if !defined(__TASKMANAGERSH__)
#define __TASKMANAGERSH__

// shell command tasks are in the range 208-239
#define READLINE_TASK 239
#define SHELL_TASK 238
#define ED_TASK 237
#define READINTOPASTEBUFFER_TASK 236
#define REBOOT_TASK 235
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
