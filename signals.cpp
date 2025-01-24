#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    // TODO: Add your implementation
    cout << "smash: got ctrl-Z" << endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t curr_pid = smash.get_currPid();
    if (curr_pid != 0) {
        smash.jobs->addJob(smash.cmd_line, true, curr_pid);
        if (kill(curr_pid, SIGSTOP) == -1) {
            perror("smash error: kill failed");
        }
        smash.set_Pid(0);
        cout << "smash: process " << curr_pid << " was stopped" << endl;
    }
}

void ctrlCHandler(int sig_num) {
  // TODO: Add your implementation
    cout << "smash: got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t curr_pid = smash.get_currPid();
    if (curr_pid != 0) {
        if (kill(curr_pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        }
        smash.set_Pid(0);
        cout << "smash: process " << curr_pid << " was killed" << endl;
    }
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

