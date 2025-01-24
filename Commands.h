#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <set>
#include "time.h"
#include <algorithm>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <sched.h>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define COMMAND_MAX_CHARS 100
#define READ 0
#define WRITE 1
#define STDDER 2

enum JobeState{
   STOPPED,
   WAITING,
   UNINITIALIZED
};

typedef enum {
    SIMPLEPIPE,
    NOTSIMPLEPIPE
}pipe_kind;

class JobsList;
//================================================================
// AUX FUNCS DECLAERTIONs
//================================================================
std::vector<std::string> storeCommandInVector(const char* cmd_line);
int _parseCommandLine(const char* cmd_line, char** args);
bool _isBackgroundComamnd(const char* cmd_line);
void _removeBackgroundSign(char* cmd_line);
std::string _trim(const std::string& s);
//================================================================
// CLASS Command
//================================================================
class Command {
public:
    const char* cmd_line;
    Command(const char* cmd_line) : cmd_line(cmd_line){}
    virtual ~Command(){};
    virtual void execute() = 0;
    virtual void prepare() {}
    virtual void cleanup() {}
};
//================================================================
// CLASS BuiltInCommand
//================================================================
class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* cmd_line) : Command(cmd_line){}
    virtual ~BuiltInCommand() = default;
};
//================================================================
// CLASS ExternalCommand
//================================================================
class ExternalCommand : public Command {
public:
    ExternalCommand(const char* cmd_line) : Command(cmd_line){}
    virtual ~ExternalCommand() {}
    void execute() override;
};
//================================================================
// CLASS JobsList
//================================================================
class JobsList {
public:
  class JobEntry {
  public:
      pid_t job_id;
      pid_t process_id;
      time_t time_stamp;
      std::string cmd_line;
      JobeState job_state;
      
      JobEntry(pid_t job_id, pid_t process_id, std::string cmd_line, JobeState job_state): job_id(job_id), process_id(process_id),
      time_stamp(), cmd_line(cmd_line), job_state(job_state) {
          time(&time_stamp);
      }
      bool operator==(const JobEntry &jobEntry) const {
          return job_id == jobEntry.job_id && process_id == jobEntry.process_id;
      }

      bool operator!=(const JobEntry &jobEntry) const {
          return !this->operator == (jobEntry);
      }

      bool operator>(const JobEntry &jobEntry) const {
          return job_id > jobEntry.job_id;
      }

      bool operator<(const JobEntry &jobEntry) const {
          return !this->operator>(jobEntry);
      }
  };
    pid_t next_id;
    std::vector<JobEntry> jobs;
    std::vector<JobEntry> finishedJobs;
    JobsList() :next_id(-1), jobs(), finishedJobs() {}
    ~JobsList() = default;
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry * getJobById(int jobId);
    void removeJobById(int jobId);
    JobEntry * getLastJob(int* lastJobId);
    JobEntry *getLastStoppedJob(int *jobId);
    void addJob(std::string cmd_line, bool isStopped, pid_t process_id);
};
//================================================================
// CLASS SmallShell
//================================================================
class SmallShell {
private:
  // TODO: Add your data members
    std::string prompt = "smash";
    std::string prev_path;
    std::string curr_path;
    pid_t curr_pid;
public:
    std::vector<std::string> cmd_vector;
    std::string cmd_line;
    JobsList* jobs;

    SmallShell() : prev_path(""), curr_pid(0), cmd_vector(), cmd_line("") {
        char path[COMMAND_MAX_CHARS];
        getcwd(path, COMMAND_MAX_CHARS);
        std::string str_path(path);
        curr_path = str_path;
        
        jobs = new JobsList();
    }
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    static SmallShell& getInstance() {// make SmallShell singleton
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
    ~SmallShell();
    void executeCommand(const char* cmd_line);
    
    void changePrompt(const std::string new_prompt) {
        prompt = new_prompt;
    }
    std::string getPrompet() const {
        return prompt;
    }
    std::vector<std::string> getCommand() {
        return cmd_vector;
    }
    std::string getPrevPath() const{
        return prev_path;
    }
    std::string getCurrPath() const{
        return curr_path;
    }
    void setPrevPath(std::string new_path) {
        prev_path = new_path;
    }
    void setCurrPath(std::string new_path) {
        curr_path = new_path;
    }
    void set_Pid(pid_t new_pid) {
        curr_pid = new_pid;
    }
    int get_currPid() const{
        return curr_pid;
    }
};
//================================================================
// CLASS RedirectionCommand
//================================================================
class RedirectionCommand : public Command {
    std::string redir_cmd;
    std::string file_name;
    int saved_stdout = 0;
    int FDT_entry = 0;
    std::string actual_cmd;
public:
    RedirectionCommand(const char* cmd_line, std::string file_name, std::string redir_cmd) : Command(cmd_line){
        this->file_name = file_name;
        this->redir_cmd = redir_cmd;
    }
    virtual ~RedirectionCommand() {}
    void execute() override;
    void prepare() override;
    void cleanup() override;
};
//================================================================
// CLASS PipeCommand
//================================================================
class PipeCommand : public Command {
public:
    pipe_kind kind;
    std::vector<std::string> pipe_commands;
    explicit PipeCommand(const char *cmd_line) : Command(cmd_line), pipe_commands() {
        std::string command(cmd_line);
        unsigned long mid = command.find("|");
        int ax = 1;
        std::string axString = "";
        if (_isBackgroundComamnd(cmd_line))
            axString = "&";
        if (command.find("|&") != std::string::npos) {
            ax++;
            kind = NOTSIMPLEPIPE;
        } else {
            kind = SIMPLEPIPE;
        }
        pipe_commands.push_back(_trim(command.substr(0, mid)) + axString);
        pipe_commands.push_back(_trim(command.substr(ax + mid)) + axString);
    }
    virtual ~PipeCommand() {}
    void execute() override;
};
//================================================================
// CLASS SetcoreCommand
//================================================================
class SetcoreCommand : public BuiltInCommand {
    // Optional
    // TODO: Add your data members
public:
    SetcoreCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};
    virtual ~SetcoreCommand() {}
    void execute() override;
};
//================================================================
// BuiltInCommands CLASSES
//================================================================
class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}
    virtual ~ChangeDirCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
public:
    QuitCommand(const char* cmd_line, JobsList* jobs)  : BuiltInCommand(cmd_line){}
    virtual ~QuitCommand() {}
    void execute() override;
};
 
class KillCommand : public BuiltInCommand {
public:
    KillCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}
    virtual ~KillCommand() {}
    void execute() override;
};

class JobsCommand : public BuiltInCommand {
public:
    JobsCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~JobsCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
public:
    ForegroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
public:
    BackgroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~BackgroundCommand() {}
    void execute() override;
};
//================================================================
// optional NOT implemented
//================================================================
/*
class TimeoutCommand : public BuiltInCommand {
// Optional
// TODO: Add your data members
 public:
  explicit TimeoutCommand(const char* cmd_line);
  virtual ~TimeoutCommand() {}
  void execute() override;
};

class FareCommand : public BuiltInCommand {
  // Optional
  // TODO: Add your data members
 public:
  FareCommand(const char* cmd_line);
  virtual ~FareCommand() {}
  void execute() override;
};
*/


#endif //SMASH_COMMAND_H_
