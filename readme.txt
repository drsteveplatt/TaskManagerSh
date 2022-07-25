A brief description of TaskManagerSh

TaskManagerSh is a command-line shell for TaskManager.  It runs on ESP units only.
It also uses SPIFFS for data/file storage.

Update:  There is an AVR version.  It only has the 'reboot' command, but can be used 
as the core for an interactive system.

It provides a command shell for an ESP TaskManager-based application.

In the future, there may be a SPIFFS-free version that will also run on AVR systems.
It may be based on OSFS.  We'll target the Mega; Nanos are too small for this kind of work.

The TaskManagerSh provides the following builtin commands:
  * ls -- list all files
  * cat fil -- display the contents of a file
  * echoto fil text -- write a line to a file (delete contents of file)
  * append fil text -- append a line to a file
  * mv f1 f2 -- rename f1 to f2
  * rm fil -- delete the file
  * cp f1 f2 -- copy f1 to f2
  * format -- reformat the filesystem
  * appendfile f1 f2 -- append the contents of f1 to f2
  * get -- get a file from a web server. not implemented yet
  * put -- put a file to a web server. not implemented yet
  * reboot -- reboot this node
  * reflash -- reload the program from a web source. not implemented yet
  * ed fn -- edit a local file using the line editor
  
  The program can also add its own commands.  The user-defined command processing 
  task(s) will receive all of the command line parameters.
  
The line editor has the following commands
    r fil -- read a file
    w [fil] -- write to either the current file or the specified file.
        the specified file will become the current file
    ? -- get info on the current file
    q -- quit
    t [start [end]] print the specified line(s).  default is the current line
    ta -- print entire file
    tw [num] -- print current line +/- num lines (window around current line)
    ib [line] -- insert before the specified line (default: current line).
        Keep inserting until a line with a single . is entered.
        Enter ".." at the start of the line for a line with a single ".".  "..." for "..", etc.
        Current line is updated to the last of the entered lines.
    ia [line] -- insert after the specified line (default: current line).
        Same entry process as for ib.
    pb [line] -- paste the paste buffer before the specified line
    pa [line] -- paste the paste buffer after the specified line
    d [start [end]] -- delete the specified line(s); save in the paste buffer.  Default: current line.
    c [start [end]] -- copy the specified line(s) to the paste buffer.
    u -- undo the last delete.
    s str1 str2 [start [end]] -- change txt1 to txt2 in the specified range of lines.
        str can be a simple word or a "longer string" ("" to have " in string)
    f str1 [start [end]] -- find the first occurrence of txt1 starting with the specified line
    b str [start] -- backwards-find for the first occurence of txt1 starting with the specified line
    g n -- go to line n
    + n -- move down n lines
    - n -- move up n lines
	
Using TaskManagerSh
	#include <Streaming.h>
	#include <vector>
	#include <FS.h>
	#include <SPIFFS.h>
	#include <TaskManager.h>
	#include <TaskManagerSh.h>
	
	// a user-defined task
	#define COMMANDTASKID 10
	
	void setup() {
		...
		TaskMgrSh.begin();	// start the shell and all of its subtasks
		TaskMgrSh.addCommand(COMMANDTASKID, "cmd", cmdTask);
		... more user commands as needed
	}
	
	void cmdTask() {
	    ... see next section
	}
	
Writing a Command Task
	Each command is an independent subtask in the TaskManager application.
		#define COMMANDTASKID 10
		void cmdTask() {
			TM_BEGINSUB_P(ShParam, myParam);
			// run your command.
			// myParam will be a local variable of type ShParam.
			// It has two fields, int* Argc (number of args to the command)
			// and vector<String>* Argv (the args; Argv[0] is the command)
			
			// If you need to interact with the user, 
			//   static string buf;
			//	 static ReadlineParam(&buf) myParam;
			//	 TM_CALL_P(some_int, READLINE_TASK, myParam);
			//	 ...on return, buf will have the string that was read in
			//   ...you can tokeinze the string if needed as well
			//   static int myArgc;
			//   static vector<String> myArgv;
			//   readlineBufTokenize(buf, myArgc, myArgv);
			TM_ENDSUB();
		}


