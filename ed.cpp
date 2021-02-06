/******************************************************************************

ed -- quick and dirty line oriented text editor

lines are numbered starting at 1.  line 0 is "first line in file";
line -1 is "last line in file"

commands supported
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



*******************************************************************************/
//#define OLDCODE

#include <arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <TaskManagerESPSub.h>

#include <vector>
using namespace std;

#include <TaskManagerSh.h>

#include <Streaming.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>
using namespace std;

// State of the editing buffer
static vector<String> theData;
static int currentLine;     // note: this is the PHYSICAL line (0..n-1), not the LOGICAL line (1..n).
                            // note also:  -1 means "past the last line", for empty files or
                            //  when you delete the last group of lines.
static vector<String> pasteBuffer;
static int deleteSource;   // start line of wherever the paste buffer was grabbed from.

static String cmdLine;
static int clCurPos;

static String currentFilename;
static bool fileModified;

// WORLD'S WORST UNDO MECHANISM
class UndoBuffer {
    public:
        vector<String> undoTheData;
        int undoCurrentLine;
        vector<String> undoPasteBuffer;
        int undoDeleteSource;
    private:
        bool undoBufferEmpty;
    public:
        UndoBuffer(): undoBufferEmpty(true) {}
        void save() {
            undoTheData.clear();
            undoPasteBuffer.clear();
            undoTheData = theData;
            undoCurrentLine = currentLine;
            undoPasteBuffer = pasteBuffer;
            undoDeleteSource = deleteSource;
            undoBufferEmpty = false;
        }
        void restore() {
            if(!undoBufferEmpty) {
                theData.clear();
                pasteBuffer.clear();
                theData = undoTheData;
                currentLine = undoCurrentLine;
                pasteBuffer = undoPasteBuffer;
                deleteSource = undoDeleteSource;
                undoBufferEmpty = true;
            }
        }
};

// FILE READ/WRITE CODE
String addSlash(const char* fn);

static bool readTheFile(const char* fn) {
    String fntmp = fn;
    String line;

    int ch;
    File f = SPIFFS.open(addSlash(fn).c_str(), FILE_READ);
    if(!f || f.isDirectory()) return false;

    theData.clear();
    line = "";

    while((ch=f.read())!=-1) {
        if(ch=='\n') {
            theData.push_back(line);
            line = "";
        } else line += char(ch);
    }
    if(line.length()>0) theData.push_back(line);
    f.close();
    return true;
}

static bool writeTheFile(const char* fn) {
  File f = SPIFFS.open(addSlash(fn).c_str(), FILE_WRITE);
  if(!f || f.isDirectory()) return false;

    for(int i=0; i<theData.size(); i++) {
        f.printf("%s\n",theData[i].c_str());
    }
    f.close();
    return true;
}

// *** END of systems interface routines

static void lineNumFix(int res, int& l1, int& l2, bool currentLineDefault=false) {
    // fixes l1 and l2 wrt order, -1 values
    // this is messy because of the way the user can enter -1 and
    // enter the numbers out of order.
    // Also note that if res==0 (nothing entered) l1,l2 is set based on currentLineDefault
    // (false->use [1, size]; true->use [currentline,currentline])
    if(res==0) {
        if(currentLineDefault) { l1=l2=currentLine; }
        else { l1=1; l2=theData.size(); }
    }
    else if(res==1) {
        if(l1==-1) l1=theData.size();
        l2=l1;
    } else {
        if(l1==-1) l1 = theData.size();
        if(l2==-1) l2 = theData.size();
        if(l1>l2) {
          int tmp; tmp=l1; l1=l2; l2=tmp;
        }
    }
}

inline bool lineNumsGood(int line1, int line2) {
  // makes sure the two values are in the range [1 theData.size()]
  return line1>=1 && line2>=1 && line1<=theData.size() && line2<=theData.size();
}

static int peelNumber(String word, int& num, bool allowStar=true) {
    // Peels a number off of cmdLine.  Advances clCurPos past the number
    // Returns 0 if no number found, 1 if a number found, -1 if a non-numeric thing found
    // Assumes spaces have been pre-peeled.  Does not peel trailing spaces.
    char tbuf[word.length()+1];
    int ret;
    if(word.length()==0) return 0; // "" is a parameter of length 0...
    if(allowStar && word=="*") {
      ret = 1;
      num = currentLine;
    }
    else { ret = sscanf(word.c_str(),"%d%s",&num,tbuf); }
    return ret==1 ? 1 : -1;
}

static int peelTwoNumbers(String word1, String word2, int& n1, int& n2) {
    // Peels one or two numbers off of cmdLine. Advances clCurPos past whatever was read.
    // Returns 0 if no numbers were found, 1 if one number was found, 2 if two numbers were found.
    // Returns -1 if either object was not a well-formed number.
    // Assumes spaces have been pre-peeled.  Does not peel trailing spaces.
    int n;
    // process first potential number
    n = peelNumber(word1, n1);
    if(n!=1) return n;  // nothing there or first was in error
    n = peelNumber(word2, n2);
    return n==-1 ? -1 : n+1;    // account for first number to return 1 or 2
}

void readIntoPasteBufferTask() {
    TM_BEGINSUB();
    // read lines into the paste buffer until a line with "." is entered.
    // Enter .. for ., ... for .., etc.
    // Any time you need "." to start, add another "."
    static String tmpLine;
    static ReadlineParam rp(&tmpLine);
    static bool done;
    done = false;
    // clear the pastebuffer
    pasteBuffer.clear();

    // read lines and append until "." is hit
    while(!done) {
        TM_CALL_P(1, READLINE_TASK, rp);
        if(tmpLine==".") done = true; // single dot line is ignored
        else {
          if(tmpLine[0]=='.' && tmpLine[1]=='.') tmpLine = tmpLine.substring(1);
          pasteBuffer.push_back(tmpLine);
        }
    }
    TM_ENDSUB();
}

static void insertPasteBufferBefore(int insertPoint) {
    // insert the paste buffer before the given line.
    // insertPoint==0 then insert at the end (append)
    // insertPoint>=theData.size() or insertPoint==-1 then just insert at the end
    vector<String>::iterator ipIt;
    if(insertPoint>=theData.size() || insertPoint==-1) {
        for(int i=0; i<pasteBuffer.size(); i++) theData.push_back(pasteBuffer[i]);
    } else {
        // insert before insertPoint
        for(int i=0; i<pasteBuffer.size(); i++) {
            ipIt = theData.begin()+insertPoint+i;
            theData.insert(ipIt, pasteBuffer[i]);
        }
    }
}
// *** END of fine-tuning for ESP

void edTask() {
    TM_BEGINSUB_P(ShParam, shParam);
    static ReadlineParam rp(&cmdLine);
    static vector<String> Argv;
    static int Argc;
    static String fn;
    static bool ret;
    static UndoBuffer undoBuffer;
    static bool done;
    // things used while parsing lines
    static int line1, line2, res;
    static int insertPoint;

    // If we were passed a file, read it in
    if(*(shParam.Argc) == 2) {
      // have a file, read it in
      if( (*(shParam.Argv))[1].length()==0) { printf("Syntax: ed fn\n"); }
      else if(!readTheFile((*(shParam.Argv))[1].c_str())) { printf("Can't read file [%s]\n", (*(shParam.Argv))[1].c_str()); }
      else { currentFilename = (*(shParam.Argv))[1]; fileModified = true; currentLine = 1; }
    }

    done = false;
    while(!done) {
        Serial.print("ed: ");
        TM_CALL_P(1, READLINE_TASK, rp);
        readlineBufTokenize(cmdLine, Argc, Argv);
        if(Argc==0) { continue; } // empty line
        else if(Argv[0]=="?") {
            if(Argc>1) { printf("Syntax: ?\n"); continue; }
            else printf("Filename: [%s].  Number of lines: %ld. Current line is %d\n", currentFilename.c_str(), theData.size(), currentLine);
        } else if(Argv[0]=="+") {
            if(Argc!=2) { printf("Syntax: + num\n"); continue; }
            res = peelNumber(Argv[1], line1);
            if(res==-1 || line1<-1 || line1==0) { printf("Syntax: + nlines\n"); continue; }
            if(line1==-1 || line1>theData.size()) line1 = theData.size();
            currentLine = min((int)theData.size(),currentLine+line1);
        } else if(Argv[0]=="-") {
            // - num
            if(Argc!=2) { printf("Syntax: - num\n"); continue; }
            res = peelNumber(Argv[1],line1);
            if(res==-1 || line1<-1 || line1==0) { printf("Syntax: - nlines\n"); continue; }
            if(line1==-1 || line1>theData.size()) line1 = theData.size();
            currentLine = max(1,currentLine-line1);
        } else if(Argv[0]=="c") {
            // c [line1 [line2]] -- copy to pastebuffer
            // Does not modify currentLine.
            int n;
            if(Argc==1) { res = 0; line1 = line2 = currentLine; }
            else if(Argc==2) { res = peelNumber(Argv[1], line1); line2 = line1; }
            else if(Argc==3) { res = peelTwoNumbers(Argv[1], Argv[2], line1, line2); }
            else { printf("Syntax: c [line1 [line2]]\n"); continue; }
            if(res==-1 || !lineNumsGood(line1, line2)) { printf("Syntax: c [line1 [line2]]\n"); continue; }
            pasteBuffer.clear();
            for(n=line1; n<=line2; n++) {
                pasteBuffer.push_back(theData[n-1]);
            }
        } else if(Argv[0]=="d") {
            // d [line1 [line2]] -- delete lines to pastebuffer
            // After the delete, curentline is set to the insert-before point.
            // (the line immediately after the deleted block)
            int n;
            if(Argc==1) { res = 0; line1 = line2 = currentLine; }
            else if(Argc==2) { res = peelNumber(Argv[1], line1); line2 = line1; }
            else if(Argc==3) { res = peelTwoNumbers(Argv[1], Argv[2], line1, line2); }
            else { printf("Syntax: d [line1 [line2]]\n"); continue; }
            lineNumFix(res, line1, line2);
            if(res==-1 || !lineNumsGood(line1, line2)) { printf("Syntax: d [line1 [line2]]\n"); continue; }
            if(line1==line2) printf("Deleting line %d\n", line1);
            else printf("Deleting %d through %d to pastebuffer\n", line1, line2);
            undoBuffer.save();
            pasteBuffer.clear();
            for(n=line1; n<=line2; n++) {
                pasteBuffer.push_back(theData[line1-1]);
                theData.erase(theData.begin()+line1-1);   // some day, (line1, line2+1)
            }
            currentLine = line1;
            if(currentLine>theData.size()) currentLine=-1;
        } else if(Argv[0]=="f") {
            // f str1 [line1 [line2]] -- find first occurrence of str
            int n;
            bool found;
            if(Argc<2 || Argc>4) { printf("Syntax: f str [line1 [line2]]\n"); continue; }
            if(Argc==2) {
              res = 0; line1 = line2 = currentLine;
            } else if(Argc==3) {
              res = peelNumber(Argv[2], line1); line2 = line1;
            } else { // Argc==4
              res = peelTwoNumbers(Argv[2], Argv[3], line1, line2);
            }
            if(res==0 && currentLine==-1) { printf("At eof, no line to search.\n"); continue; }
            lineNumFix(res, line1, line2);
            if(res==-1 || !lineNumsGood(line1, line2)) {
                printf("Syntax: f strOld [line1 [line2]]\n");
                continue;
            }
            found = false;
            for(n=line1; n<=line2 && !found; n++) {
                if((theData[n-1].indexOf(Argv[1]))!=-1) {
                    currentLine = n;
                    found = true;
                }
            }
            // Note: subtract an extra -1 because n has been incremented before the exit test.
            if(found) { printf("*%3d: %s\n", n-1, theData[n-1-1].c_str()); }
            else { printf("Search string not found.\n"); }
        } else if(Argv[0]=="g") {
            // g line
            if(Argc!=2) { printf("Syntax: g line\n"); continue; }
            res = peelNumber(Argv[1], line1);
            if(res==-1 || line1<-1 || line1==0) { printf("Syntax: g line\n"); continue; }
            if(line1==-1 || line1>theData.size()) line1 = theData.size();
            currentLine = line1;
        } else if(Argv[0]=="h") {
            // help -- list the commands
            printf("r w -- file read/write\n");
            printf("q ? h -- quit, info, help\n");
            printf("g + - -- goto line; go forward or backwards by lines\n");
            printf("t ta tw -- type lines: specific, all, window around current line\n");
            printf("f s sa -- find a string; substitute first/all occurrences of a string\n");
            printf("d c -- delete or copy lines to pastebuffer\n");
            printf("ia ib pa pb -- insert new | paste pastebuffer after/before current line\n");
            printf("u -- undo last operation that modified the text\n");
        } else if(Argv[0]=="ia") {
            // ia [line] -- insert after
            int n;
            if(Argc>2) { printf("Syntax: ia [line]\n"); continue; }
            if(Argc==1) { res = 1; line1 = currentLine; }
            else {
              res = peelNumber(Argv[1],line1);
              if(res!=1) { printf("Syntax: ia [line1]\n"); continue; }
            }
            // set insertPoint to the point we insert lines BEFORE
            // This will be an index into theData.  It is not a user-line-number.
            // insertPoint==-1 means insert at the end
            if(res==0) insertPoint = currentLine;     // noting entered, use current line
            else {
                if(line1==-1) insertPoint = -1;      // marker for "at the end, not before anything"
                else insertPoint = line1;
            }
            undoBuffer.save();
            TM_CALL(3, READINTOPASTEBUFFER_TASK);
            if(pasteBuffer.size()==0) continue;
            insertPasteBufferBefore(insertPoint);
            currentLine = insertPoint==-1 ? theData.size()-pasteBuffer.size() : insertPoint+1;
        } else if(Argv[0]=="ib") {
            // ib [line] -- insert before
            int n;
            if(Argc>2) { printf("Syntax: ia [line]\n"); continue; }
            if(Argc==1) { res = 1; line1 = currentLine; }
            else {
              res = peelNumber(Argv[1],line1);
              if(res!=1) { printf("Syntax: ia [line1]\n"); continue; }
            }
            // set insertPoint to the point we insert lines BEFORE
            // This will be an index into theData.  It is not a user-line-number.
            // insertPoint==-1 means insert at the end
            if(res==0) insertPoint = (currentLine==-1 ? -1 : currentLine-1);    // noting entered, use current line
            else if(line1==-1) insertPoint = theData.size()-1;
            else insertPoint = line1-1;
            undoBuffer.save();
            TM_CALL(2, READINTOPASTEBUFFER_TASK);
            if(pasteBuffer.size()==0) continue;
            insertPasteBufferBefore(insertPoint);
            currentLine = insertPoint==-1 ? theData.size()-pasteBuffer.size() : insertPoint;
        } else if(Argv[0]=="pa") {
            // pa [line] -- paste after
            // sets currentLine to the first line of the inserted block.
            int n;
            if(Argc>2) { printf("Syntax: ia [line]\n"); continue; }
            if(Argc==1) { res = 1; line1 = currentLine; }
            else {
              res = peelNumber(Argv[1],line1);
              if(res!=1) { printf("Syntax: ia [line1]\n"); continue; }
            }
            if(pasteBuffer.size()==0) continue;
            // set insertPoint to the point we insert lines BEFORE
            // This will be an index into theData.  It is not a user-line-number.
            // insertPoint==-1 means insert at the end
            if(res==0) insertPoint = currentLine;     // noting entered, use current line
            else {
                if(line1==-1) insertPoint = -1;      // marker for "at the end, not before anything"
                else insertPoint = line1;
            }
            undoBuffer.save();
            insertPasteBufferBefore(insertPoint);
            currentLine = insertPoint==-1 ? theData.size()-pasteBuffer.size() : insertPoint+1;
        } else if(Argv[0]=="pb") {
            // pb [line] -- paste before
            // Sets currentLine to the first line of the pasted block.
            int n;
            if(Argc>2) { printf("Syntax: ia [line]\n"); continue; }
            if(Argc==1) { res = 1; line1 = currentLine; }
            else {
              res = peelNumber(Argv[1],line1);
              if(res!=1) { printf("Syntax: ia [line1]\n"); continue; }
            }
            if(pasteBuffer.size()==0) continue;
            // set insertPoint to the point we insert lines BEFORE
            // This will be an index into theData.  It is not a user-line-number.
            // insertPoint==-1 means insert at the end
            if(res==0) insertPoint = (currentLine==-1 ? -1 : currentLine-1);    // noting entered, use current line
            else if(line1==-1) insertPoint = theData.size()-1;
            else insertPoint = line1-1;
            undoBuffer.save();
            insertPasteBufferBefore(insertPoint);
            currentLine = insertPoint==-1 ? theData.size()-pasteBuffer.size() : insertPoint+1;
        } else if(Argv[0]=="q") {
            if(Argc!=1) { printf("Syntax: q\n"); continue; }
            else done = true;
        } else if(Argv[0]=="r") {
            // r filename //*****HERE*****
            if(Argc==1 || Argc>2) { printf("Syntax: r fn\n"); continue; }
            undoBuffer.save();
            if(Argv[1].length()==0) { printf("Syntax: r fn\n"); continue; }
            else if(!readTheFile(Argv[1].c_str())) { printf("Can't read file [%s]\n", Argv[1].c_str()); }
            else { currentFilename = Argv[1]; fileModified = true; currentLine = 1; }
        } else if(Argv[0]=="s") {
            // s str1 str2 [line1 [line2]] -- substitute -- replace str1 with str2 once
            int n;
            int pos;
            bool found;
            if(Argc<3 || Argc>5) { printf("Syntax: s strOld strNew [line1 [line2]]\n"); continue; }
            if(Argc==3) { line1 = line2 = currentLine; res = 2; }
            else if(Argc==4) { res = peelNumber(Argv[3], line1); if(res==1) line2=line1; }
            else res = peelTwoNumbers(Argv[3], Argv[4], line1, line2);
            if(res==0 && currentLine==-1) { printf("At eof, no line to search.\n"); continue; }
            lineNumFix(res, line1, line2);
            if(res==-1 || !lineNumsGood(line1, line2)) { printf("Syntax: s strOld strNew [line1 [line2]]\n"); continue; }
            found = false;
            undoBuffer.save();
            for(n=line1; n<=line2 && !found; n++) {
                if((pos=theData[n-1].indexOf(Argv[1]))!=-1) {
                    theData[n-1] = theData[n-1].substring(0,pos) + Argv[2] + theData[n-1].substring(pos+Argv[1].length());
                    found = true;
                    currentLine = n;
                }
            }
        } else if(Argv[0]=="sa") {
            // sa str1 str2 [line1 [line2]] -- substitute all -- replace all str1 with str2
            //string word1, word2;
            int n;
            int linePos, matchLoc, pos;
            if(Argc<3 || Argc>5) { printf("Syntax: s strOld strNew [line1 [line2]]\n"); continue; }
            if(Argc==3) { res = peelNumber(Argv[3], line1); line2 = line1; }
            else { res = peelTwoNumbers(Argv[3], Argv[4], line1, line2); }
            lineNumFix(res, line1, line2);
            if(res==-1 || !lineNumsGood(line1, line2)) { printf("Syntax: s strOld strNew [line1 [line2]]\n"); continue; }
            undoBuffer.save();
            for(n=line1; n<=line2; n++) {
                linePos = 0;    // allow for multiple matches in the line
                while((pos = theData[n-1].indexOf(Argv[1],linePos))!=-1) {
                    theData[n-1] = theData[n-1].substring(0,pos) + Argv[2] + theData[n-1].substring(pos+Argv[1].length());
                    linePos = pos + Argv[2].length();
                    currentLine = n;
                }
            }
        } else if(Argv[0]=="t") {
            // t [line1 [line2]]
            int n;
            if(Argc==1 || Argc>3) { printf("Syntax: t [line1 [line2]]\n"); continue; }
            if(Argc==2) {
              res = peelNumber(Argv[1], line1); line2 = line1;
              if(res!=1) { printf("Syntax: t [line1 [line2]]"); continue; }
            } else {
              res = peelNumber(Argv[2], line2); if(res==1) res=2; // force res to -1 0 2
              if(res!=2) { printf("Syntax: t [line1 [line2]]"); continue; }
            }
            lineNumFix(res, line1, line2, true);
            if(res==-1  || !lineNumsGood(line1, line2)) { printf("Syntax: t [line1 [line2]]"); continue; }
            if(!(line1==currentLine&&line2==currentLine) && (line1<1 || line2>theData.size())) { printf("Line number out of range.\n"); continue; }
            for(n=line1; n<=theData.size() && n<=line2; n++) {
                printf("%c%.3d: %s\n", n==currentLine?'*':' ', n, theData[n-1].c_str());
            }
        } else if(Argv[0]=="ta") {
            // ta
            int n;
            if(Argc!=1) { printf("Syntax: ta\n"); continue; }
            for(n=1; n<=theData.size(); n++) {
                printf("%c%.3d: %s\n", n==currentLine?'*':' ', n, theData[n-1].c_str());
            }
        } else if(Argv[0]=="tw") {
            // tw [num]
            int nLines;
            int n;
            int tmpCurrentLine;
            if(Argc>2) { printf("Syntax: tw [line]\n"); continue; }
            if(Argc==1) { res = 1; nLines = 1; }
            else {
              res = peelNumber(Argv[1], nLines);
              if(res!=1) { printf("Syntax: tw [line1]\n"); continue; }
            }
            tmpCurrentLine = currentLine==-1 ? theData.size()+1 : currentLine;
            line1 = max(1, tmpCurrentLine-nLines);
            line2 = min((int)theData.size(), currentLine+nLines);
            for(n=line1; n<=line2; n++) {
                printf("%c%.3d: %s\n", n==currentLine?'*':' ', n, theData[n-1].c_str());
            }
        } else if(Argv[0]=="u") {
            // u -- undelete lines
            undoBuffer.restore();
        } else if(Argv[0]=="w") {
            // w [filename]
            if(Argc==1) fn = currentFilename;
            else if(Argc==2) fn = Argv[1];
            else { printf("Syntax: w [fn]\n"); continue; }
            if(fn.length()==0) { printf("Syntax: w [fn]\n"); continue; }
            if(!writeTheFile(fn.c_str())) { printf("Can't write file[%s]\n", fn.c_str()); }
            else fileModified = false;
        } else {
            printf("unknown command.\n");
        }

    }
    // clean up
    theData.clear();
    pasteBuffer.clear();
    currentFilename = "";
    TM_ENDSUB();
}
