#include <string.h>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif
//================================================================
// AUX FUNCS DECLAERTIONs
//================================================================
set<string> BuiltInCommandsSet{"cd", "chprompt", "showpid", "pwd", "jobs", "fg", "bg", "quit", "kill"};
string _ltrim(const std::string& s);
string _rtrim(const std::string& s);
string _trim(const std::string& s);
int _parseCommandLine(const char* cmd_line, char** args); // NOT USED
bool _isBackgroundComamnd(const char* cmd_line);
void _removeBackgroundSign(char* cmd_line);
vector<string> storeCommandInVector(const char* cmd_line);
bool isNumber(string &str);
void convertStringVecToCharVec(vector<string> str_vec, char** char_vec);
//================================================================
// SmallShell::CreateCommand
//================================================================
Command * SmallShell::CreateCommand(const char* cmd_line) {

    vector<string> cmd_vec = storeCommandInVector(cmd_line);

    if(string(cmd_line).find("|") != string::npos) {
        return new PipeCommand(cmd_line);
    }
    if(string(cmd_line).find(">>") != std::string::npos) {
        string file_name = string(cmd_line).substr(string(cmd_line).find(">>") + 2);
        file_name = _trim(file_name);
        return new RedirectionCommand(cmd_line, file_name, ">>");
    }
    if(string(cmd_line).find(">") != std::string::npos) {
        string file_name = string(cmd_line).substr(string(cmd_line).find(">") + 1);
        file_name = _trim(file_name);
        return new RedirectionCommand(cmd_line, file_name, ">");
    }
    string cmd = cmd_vector[0];
    if (cmd == "chprompt") {
        return nullptr;
    }
    if (cmd == "showpid") {
        return new ShowPidCommand(cmd_line);
    }
    if (cmd == "pwd") {
        return new GetCurrDirCommand(cmd_line);
    }
    if (cmd == "cd") {
        return new ChangeDirCommand(cmd_line);
    }
    if (cmd == "jobs") {
        return new JobsCommand(cmd_line);
    }
    if (cmd == "fg") {
        return new ForegroundCommand(cmd_line);
    }
    if (cmd == "bg") {
        //return new BackgroundCommand(cmd_line);
        cerr << "smash error: execvp failed: No such file or directory" << endl;
        return nullptr;
    }
    if (cmd == "quit") {
        return new QuitCommand(cmd_line, jobs);
    }
    if (cmd == "kill") {
        return new KillCommand(cmd_line);
    }
    if (cmd == "setcore") {
        return new SetcoreCommand(cmd_line);
    }
    return new ExternalCommand(cmd_line);
}
//================================================================
// SmallShell::executeCommand
//================================================================
void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line);
    if (cmd == nullptr) {
        return;
    }
    jobs->removeFinishedJobs();
    cmd->execute();
    delete cmd;
}
//================================================================
// PWD - EXECUTION
//================================================================
void GetCurrDirCommand::execute() {
    char path[COMMAND_MAX_CHARS];
    getcwd(path, COMMAND_MAX_CHARS);
    string str_path(path);
    cout << str_path << endl;
}
//================================================================
// SHOWPID - EXECUTION
//================================================================
void ShowPidCommand::execute() {
    cout << "smash pid is " << getpid() << endl;
}
//================================================================
// CD - EXECUTION
//================================================================
void ChangeDirCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    vector<string> cmd_vec = smash.cmd_vector;
    if(!cmd_vec[2].empty()) {
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    }
    string pram = cmd_vec[1];
    if(pram.compare("-") == 0) {
        if (smash.getPrevPath().empty()) {
            cerr << "smash error: cd: OLDPWD not set" << endl;
        } else {
            string prev = smash.getPrevPath();
            if(chdir(prev.c_str()) == -1) {
                perror("smash error: chdir failed");
                return;
            }
            string curr = smash.getCurrPath();
            smash.setPrevPath(curr);
            smash.setCurrPath(prev);
        }
        return;
    }
    if(pram.compare("..") == 0) {
        if (chdir("..") == -1) {
            perror("smash error: chdir failed");
            return;
        }
        string prev = smash.getCurrPath();
        if (strcmp(prev.c_str(), "/") == 0) {
            return;
        }
        char path[COMMAND_MAX_CHARS];
        getcwd(path, COMMAND_MAX_CHARS);
        string curr(path);

        smash.setCurrPath(curr);
        smash.setPrevPath(prev);
        return;
    }
    char path[COMMAND_MAX_CHARS];
    getcwd(path, COMMAND_MAX_CHARS);
    string prev(path);

    if(chdir(pram.c_str()) == -1) {
        perror("smash error: chdir failed");
        return;
    }
    getcwd(path, COMMAND_MAX_CHARS);
    string curr(path);

    smash.setPrevPath(prev);
    smash.setCurrPath(curr);
}
//================================================================
// JOBS - EXECUTION
//================================================================
void JobsCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    smash.jobs->printJobsList();
}
//================================================================
// FG - EXECUTION
//================================================================
void ForegroundCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    vector<std::string> cmd_vector = shell.cmd_vector;

    if(!cmd_vector[2].empty()||
            ((!cmd_vector[1].empty())&&(!isNumber(cmd_vector[1]))) )
    {
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }

    if(cmd_vector[1].empty()) {
        if(shell.jobs->jobs.empty()) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        if(cmd_vector[2].empty()) {
            JobsList::JobEntry* job = shell.jobs->getLastJob(NULL);
            bool stopped = (job->job_state==STOPPED);
            if(stopped) {
                job->job_state = WAITING;
                kill(job->process_id, SIGCONT);
            }
            shell.set_Pid(job->process_id);
            shell.cmd_line = job->cmd_line;
            shell.cmd_vector = storeCommandInVector(shell.cmd_line.c_str());
            pid_t job_pid = job->process_id;
            shell.jobs->finishedJobs.push_back(JobsList::JobEntry(job->job_id,job->process_id,job->cmd_line,
                                                                  job->job_state));
            shell.jobs->removeJobById(job->job_id);
            //cout << shell.cmd_line << " : " << shell.get_currPid() << endl;
            cout << shell.cmd_line << " " << shell.get_currPid() << endl;
            waitpid(job_pid, NULL, WUNTRACED);
            shell.set_Pid(0);
            return;
        }
        return;
    }
                JobsList::JobEntry * job = shell.jobs->getJobById(std::stoi(cmd_vector[1]));
                if(job!= nullptr) {
                    bool stopped = (job->job_state == STOPPED);
                    if (stopped) {
                        job->job_state = WAITING;
                        kill(job->process_id, SIGCONT);
                    }
                    shell.cmd_line = job->cmd_line;
                    shell.cmd_vector = storeCommandInVector(job->cmd_line.c_str());
                    shell.set_Pid(job->process_id);
                    pid_t job_pid = job->process_id;
                    shell.jobs->finishedJobs.push_back(
                            JobsList::JobEntry(job->job_id, job->process_id, job->cmd_line, job->job_state));
                    shell.jobs->removeJobById(job->job_id);
                    //cout << shell.cmd_line << " : " << shell.get_currPid() << endl;
                    cout << shell.cmd_line << " " << shell.get_currPid() << endl;
                    waitpid(job_pid, NULL, WUNTRACED);
                    shell.set_Pid(0);
                    return;
                }
        cerr << "smash error: fg: job-id " << cmd_vector[1] << " does not exist" << endl;
        return;
}

//================================================================
// BG - EXECUTION
//================================================================
bool jobCond(JobsList::JobEntry* job,int job_id)
{
    if(job == nullptr) {
        cerr << "smash error: bg: job-id " << job_id<< " does not exist" << endl;
        return false;
    }
    if(job->job_state != STOPPED) {
        cerr << "smash error: bg: job-id " << job_id << " is already running in the background" << endl;
        return false;
    }
    return true;
}


void BackgroundCommand::execute() {
    SmallShell& shell= SmallShell::getInstance();
    std::vector<std::string> cmd_vector = shell.cmd_vector;
    if(cmd_vector[0].empty() || !cmd_vector[2].empty() ||
       ((!cmd_vector[1].empty()) && (!isNumber(cmd_vector[1])))) {
        cerr << "smash error: bg: invalid arguments" << endl;
        return;
    }
    if(cmd_vector[1].empty())
    {
        if(shell.jobs->jobs.empty()) {
            cerr <<  "smash error: bg: there is no stopped jobs to resume"<< endl;
            return;
        }
        JobsList::JobEntry * job= shell.jobs->getLastStoppedJob(NULL);
        if(job != nullptr) {
            job->job_state = WAITING;
            cout << job->cmd_line << " : " << job->process_id << endl;
            kill(job->process_id, SIGCONT);
            return;
        }
        cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
        return;
    }

        int job_id = std::stoi(cmd_vector[1]);
        JobsList::JobEntry * job = shell.jobs->getJobById(job_id);
        bool cont = jobCond(job,job_id);
        if(!cont)
            return;
        job->job_state = WAITING;
        cout << job->cmd_line << " : " << job->process_id << endl;
        kill(job->process_id, SIGCONT);
        return;
}

//================================================================
// QUIT - EXECUTION
//================================================================
void QuitCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    std::vector<std::string> command_line = shell.cmd_vector;
    if(!command_line[1].empty() && command_line[1].compare("kill") == 0) {
        shell.jobs->killAllJobs();
    }
    exit(0);
}
//================================================================
// KILL - EXECUTION
//================================================================
void KillCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    vector<string> cmd_vec = smash.getCommand();
    if ((cmd_vec[1].empty()) || (cmd_vec[2].empty())/* || !(cmd_vec[3].empty())*/) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    pid_t pram2 = 0;
    if (isNumber(cmd_vec[2])) {
        pram2 = std::stoi(cmd_vec[2]);
	} else {
		cerr << "smash error: kill: invalid arguments" << endl;
		return;
	}
	
    JobsList::JobEntry* job = smash.jobs->getJobById(pram2);
    if (job == nullptr) {
        cerr << "smash error: kill: job-id "<<pram2<<" does not exist" << endl;
        return;
    }
    
    if (!(cmd_vec[3].empty())) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    
    if (cmd_vec[1][0] == '-') {
        cmd_vec[1] = cmd_vec[1].substr(cmd_vec[1].find_first_not_of("-"));
    } else {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    
    pid_t pram1 = 0;
    if (isNumber(cmd_vec[1]) && isNumber(cmd_vec[2])) {
        pram1 = std::stoi(cmd_vec[1]);
        pram2 = std::stoi(cmd_vec[2]);
    } else {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    if ((pram1 > 31)) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    /*
    JobsList::JobEntry* job = smash.jobs->getJobById(pram2);
    if (job == nullptr) {
        cerr << "smash error: kill: job-id "<<pram2<<" does not exist" << endl;
        return;
    }
    */
    pid_t job_pid = job->process_id;
    if (pram1 == SIGKILL) {
        if (kill(job_pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return ;
        }
        smash.jobs->removeJobById(pram2);
        cout << "signal number " << pram1 << " was sent to pid " << job_pid << endl;
        return;
    }

    if (pram1 == SIGSTOP) {
        if (kill(job_pid, SIGSTOP) == -1) {
            perror("smash error: kill failed");
            return;
        }
        job->job_state = STOPPED;
        cout << "signal number " << pram1 << " was sent to pid " << job_pid << endl;
        return;
    }

    if (pram1 == SIGCONT) {
        if (kill(job_pid, SIGCONT) == -1) {
            perror("smash error: kill failed");
            return;
        }
        job->job_state = WAITING;
        cout << "signal number " << pram1 << " was sent to pid " << job_pid << endl;
        return;
    }
    if (kill(job_pid, pram1) == -1) {
        perror("smash error: kill failed");
        return ;
    }
    cout << "signal number " << pram1 << " was sent to pid " << job_pid << endl;
}
/*
 void KillCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    vector<string> cmd_vec = smash.getCommand();
    if ((cmd_vec[1].empty()) || (cmd_vec[2].empty()) || !(cmd_vec[3].empty())) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    
    JobsList::JobEntry* job = smash.jobs->getJobById(pram2);
    if (job == nullptr) {
        cerr << "smash error: kill: job-id "<<pram2<<" does not exist" << endl;
        return;
    }
    
    if (!(cmd_vec[3].empty())) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    
    if (cmd_vec[1][0] == '-') {
        cmd_vec[1] = cmd_vec[1].substr(cmd_vec[1].find_first_not_of("-"));
    } else {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    
    pid_t pram1 = 0, pram2 = 0;
    if (isNumber(cmd_vec[1]) && isNumber(cmd_vec[2])) {
        pram1 = std::stoi(cmd_vec[1]);
        pram2 = std::stoi(cmd_vec[2]);
    } else {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    if ((pram1 > 31)) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    
    JobsList::JobEntry* job = smash.jobs->getJobById(pram2);
    if (job == nullptr) {
        cerr << "smash error: kill: job-id "<<pram2<<" does not exist" << endl;
        return;
    }
    
    pid_t job_pid = job->process_id;
    if (pram1 == SIGKILL) {
        if (kill(job_pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return ;
        }
        smash.jobs->removeJobById(pram2);
        cout << "signal number " << pram1 << " was sent to pid " << job_pid << endl;
        return;
    }

    if (pram1 == SIGSTOP) {
        if (kill(job_pid, SIGSTOP) == -1) {
            perror("smash error: kill failed");
            return;
        }
        job->job_state = STOPPED;
        cout << "signal number " << pram1 << " was sent to pid " << job_pid << endl;
        return;
    }

    if (pram1 == SIGCONT) {
        if (kill(job_pid, SIGCONT) == -1) {
            perror("smash error: kill failed");
            return;
        }
        job->job_state = WAITING;
        cout << "signal number " << pram1 << " was sent to pid " << job_pid << endl;
        return;
    }
    if (kill(job_pid, pram1) == -1) {
        perror("smash error: kill failed");
        return ;
    }
    cout << "signal number " << pram1 << " was sent to pid " << job_pid << endl;
}
 */
//================================================================
// External - EXECUTION
//================================================================
void ExternalCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    std::string shell_cmd_line = smash.cmd_line;
    int process_id = fork();
    if(process_id == 0) {
        if(shell_cmd_line.find("*") == std::string::npos && shell_cmd_line.find("?") == std::string::npos ) {
            setpgrp();
            char** args = new char*[smash.cmd_vector.size()+1];
            convertStringVecToCharVec(smash.cmd_vector,args);
            execvp(*args,args);
            perror("smash error: execvp failed");
            exit(1);
        } else {
            setpgrp();
            char *cmd_nonConst = new char[strlen(cmd_line) + 1];// +1 ?
            strcpy(cmd_nonConst, cmd_line);
            if (_isBackgroundComamnd(cmd_line)) {
                _removeBackgroundSign(cmd_nonConst);
            }
            char str1[10] = "/bin/bash";
            char str2[3] = "-c";
            char *const arguments[4] = {str1, str2, cmd_nonConst, NULL};
            //we might need to check what it returns
            execv("/bin/bash", arguments);
            perror("smash error: execv failed");
            exit(1);
        }
    }
    else if (_isBackgroundComamnd(cmd_line)== false) {
        smash.set_Pid(process_id);
        smash.cmd_line = cmd_line;
        waitpid(process_id, NULL, WUNTRACED);
        smash.set_Pid(0);
        smash.cmd_line = "";
    } else {
        smash.jobs->addJob(shell_cmd_line, false, process_id);
    }
}
//================================================================
// REDIRECTION
//================================================================
void RedirectionCommand::execute() {
    prepare();
    SmallShell& shell = SmallShell::getInstance();
    shell.cmd_line = string(actual_cmd);
    shell.cmd_vector = storeCommandInVector(actual_cmd.c_str());
    shell.executeCommand(actual_cmd.c_str());
    cleanup();
}

void RedirectionCommand::prepare() {
    saved_stdout = dup(1);
    if (redir_cmd.compare(">") == 0) {
        FDT_entry = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0655);
//        FDT_entry = open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC ,S_IRWXO | S_IRWXU |S_IRWXG);
    } else {
        FDT_entry = open(file_name.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0655);
//        FDT_entry = open(file_name.c_str(), O_CREAT | O_WRONLY | O_APPEND,S_IRWXO | S_IRWXU |S_IRWXG);
    }
    if (FDT_entry == -1) {
        perror("smash error: open failed");
        return;
    }
    if (close(1) == -1) {
        close(FDT_entry);
        perror("smash error: close failed");
        return;
    }
    if(dup2(FDT_entry, 1) == -1) {
        close(FDT_entry);
        perror("smash dup2: open failed");
        return;
    }
    SmallShell& shell = SmallShell::getInstance();
    actual_cmd = shell.cmd_line.substr(0, std::string(cmd_line).find(redir_cmd));
}

void RedirectionCommand::cleanup() {
    if (FDT_entry != -1){
        if(close(FDT_entry) == -1 || close(1) == -1 ) {
            perror("smash error: close failed");
        }
    }
    if (dup2(saved_stdout, 1) == -1) {
        perror("smash dup2: open failed");
        return;
    }
    close(saved_stdout);
}
//================================================================
// PIPE
//================================================================
void endPipe(int pipes[2], int f1, int f2) {
    close(pipes[WRITE]);
    close(pipes[READ]);
    if ((waitpid(f1, NULL, 0) == -1) || (waitpid(f2, NULL, 0) == -1)) {
        perror("smash error: waitpid failed");
    }
}

void PipeCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    int pipes[2];
    int channels[2];
    pid_t son1 = -1;
    pid_t son2= -1;

    if (kind == SIMPLEPIPE) {
        channels[0] = READ;
        channels[1] = WRITE;
    } else {
        channels[0] = READ;
        channels[1] = STDDER;
    }

    pipe(pipes);
    if (fork() == 0) {
        int savedFd2=dup(channels[1]);
        // int savedFd = dup(channels[1]);
        setpgrp();
        son1 = getpid();
        close(pipes[READ]);
        dup2(pipes[WRITE], channels[1]);
        close(pipes[WRITE]);
        shell.cmd_line = pipe_commands[0];
        shell.cmd_vector = storeCommandInVector(pipe_commands[0].c_str());
        Command *Command = shell.CreateCommand(pipe_commands[0].c_str());
        Command->execute();
        dup2(savedFd2,channels[1]);
        close(savedFd2);
        exit(0);
    }
    if (fork() == 0) {
        int savedFd1=dup(channels[0]);
        setpgrp();
        son2 = getpid();
        close(pipes[WRITE]);
        dup2(pipes[READ], READ);
        close(pipes[READ]);
        shell.cmd_line = pipe_commands[1];
        shell.cmd_vector = storeCommandInVector(pipe_commands[1].c_str());
        Command *Command = shell.CreateCommand(pipe_commands[1].c_str());
        Command->execute();
        dup2(savedFd1,channels[0]);
        close(savedFd1);
        exit(0);
    }
    endPipe(pipes,son1,son2);
}
//================================================================
// SETCORE
//================================================================
void SetcoreCommand::execute() {
    SmallShell& shell=SmallShell::getInstance();
    if(!shell.cmd_vector[3].empty()||!isNumber(shell.cmd_vector[2])||
       !isNumber(shell.cmd_vector[1]) )
    {
        cerr<<"smash error: setcore: invalid arguments"<<endl;
        return;
    }
    if(shell.jobs->getJobById(stoi(shell.cmd_vector[1].c_str()))== nullptr) {
        cerr<<"smash error: setcore: job-id "<<stoi(shell.cmd_vector[1].c_str())<<" does not exist"<<endl;
        return;
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(std::stoi(shell.cmd_vector[2].c_str()),&mask);

    int res = sched_setaffinity(shell.jobs->getJobById(stoi(shell.cmd_vector[1].c_str()))->process_id, sizeof(cpu_set_t), &mask);
    if(res==-1)
    {
        if(errno==EINVAL)
        {
            cerr<<"smash error: setcore: invalid core number"<<endl;
            return;
        }
        if(errno==EPERM)
        {
            cerr<<"smash error: setcore: invalid core number"<<endl;
            return;
        }
        cerr<<"smash error: setcore: invalid arguments"<<endl;
    }
}
//================================================================
// JOBS RELATED FUNCS
//================================================================
//===========================================
void JobsList::printJobsList() {
//    removeFinishedJobs();// no need!
    unsigned int i = 0;
    if(!jobs.empty()) {
        while (i < jobs.size()) {
			/*
            time_t curr_time;
            time(&curr_time);
            double time_elapsed = difftime(curr_time, jobs[i].time_stamp);
            */
            if (jobs[i].job_state == STOPPED) {
                cout << "[" << jobs[i].job_id << "] "<<jobs[i].cmd_line/*<< " : " << jobs[i].process_id << " " << time_elapsed << " secs " << "(stopped)"*/ << endl;
                i++;
                continue;
            }
            cout << "[" << jobs[i].job_id << "] "<< jobs[i].cmd_line/*<< " : " << jobs[i].process_id << " " << time_elapsed << " secs" */<< endl;;
            i++;
        }
    }
    return;
}


void killJob(JobsList::JobEntry& job)
{
    cout << job.process_id << ": "<< job.cmd_line;
    cout << endl;
    kill(job.process_id, 9);
}

int decideNext(bool empty)
{
    return (empty== true) ? 1 : 0 ;
}

void JobsList::killAllJobs() {
    cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:" << endl;
    unsigned int i = 0;
    while(i < jobs.size())
    {
        killJob(jobs[i]);
        i++;
    }
    jobs.clear();
    finishedJobs.clear();
    next_id = decideNext(true);
}

void JobsList::removeFinishedJobs() {
    if (jobs.size() == 0) {
        return;
    }
    else
    {
    for (auto it = jobs.begin(); it != jobs.end();) {
        pid_t status = -1;
        if (waitpid(it->process_id, &status, WNOHANG) > 0) {
            jobs.erase(it);
            it = jobs.begin();
        } else {
            it++;
        }}
    }

    std::sort(jobs.begin(), jobs.end());
    int x = (jobs.empty()==true) ? 1 : jobs.back().job_id + 1;
    next_id=x;
    return;
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for(unsigned int i=0; i < jobs.size(); i++)
    {
        if(jobs[i].job_id == jobId)
        {
            return &jobs[i];
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    for (auto iter = jobs.begin(); iter != jobs.end(); iter++) {
        if (iter->job_id == jobId) {
            jobs.erase(iter);
            break;
        }
    }
    sort(jobs.begin(), jobs.end());
    next_id = jobs.back().job_id+1;
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    JobsList::JobEntry* last_job = &jobs.back();
    if (lastJobId) {
        *lastJobId = jobs.back().job_id;
    }
    return last_job;
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    unsigned int i = 0;
    int wanted = -1;
    while (i < jobs.size())
    {
        if(jobs[i].job_state == STOPPED)
        {
            wanted = i;
        }
        i++;
    }
    if (jobId) {
        *jobId = jobs[wanted].job_id;
    }
    return &jobs[wanted];
}

void JobsList::addJob(string cmd_line, bool isStopped, pid_t process_id) {
    pid_t job_id = 0;
    JobeState job_state = (isStopped) ? STOPPED : WAITING;

    for (auto iter = finishedJobs.begin(); iter != finishedJobs.end(); iter++)
    {
        if(iter->process_id==process_id)
        {
            jobs.push_back(JobEntry(iter->job_id,iter->process_id,iter->cmd_line,job_state));
            finishedJobs.erase(iter);
            sort(jobs.begin(), jobs.end());
            next_id=jobs.back().job_id+1;
            return;
        }
    }

    job_id = (jobs.empty()== true) ? 1 : next_id;
    next_id++;
    jobs.push_back(JobsList::JobEntry(job_id, process_id, cmd_line, job_state));
    sort(jobs.begin(), jobs.end());
}
//================================================================
// AUX IMPLEMENTAION
//================================================================
string _ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned long idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

vector<string> storeCommandInVector(const char* cmd_line) {
    string cmd_string = string(cmd_line);
    char* cmd_nonConst = new char[strlen(cmd_line) + 1];
    strcpy(cmd_nonConst, cmd_line);
    if (_isBackgroundComamnd(cmd_line)) {
        _removeBackgroundSign(cmd_nonConst);
        cmd_string = string(cmd_nonConst);
    }
    delete [] cmd_nonConst;
    vector<string> args(20, "");
    cmd_string = _trim(string(cmd_string));
    string first_word = cmd_string.substr(0, cmd_string.find_first_of(" \n"));
    int i = 0;
    for (auto index = args.begin(); index != args.end(); index++) {
        string arg = cmd_string.substr(0, cmd_string.find_first_of(WHITESPACE));
        if (arg.empty()) {
            break;
        }
        args[i] = arg;
        cmd_string = cmd_string.substr(arg.size());
        cmd_string = _trim(string(cmd_string));
        i++;
    }
    return args;
}

bool isNumber(string &str) {
    if(str=="-")
    {
        return false;
    }
    int count_firstDigit=0;
    for (char it : str) {
        if(count_firstDigit==0) {
            if(str[0]=='-') {
                count_firstDigit++;
                continue;
            }
            count_firstDigit++;
        }
        if (std::isdigit(it) == 0)
            return false;
    }
    return true;
}

void convertStringVecToCharVec(vector<string> str_vec, char** char_vec) {
    int i ;
    for (i = 0; str_vec[i].empty() == false; i++) {
        char_vec[i] = new char[str_vec[i].size() + 1];
        strcpy(char_vec[i], str_vec[i].c_str());
    }
    char_vec[i] = nullptr;
}

// TODO: Add your implementation for classes in Commands.h
/*
SmallShell::SmallShell() {
// TODO: add your implementation
}
*/
SmallShell::~SmallShell() {
// TODO: add your implementation
}
