#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    if(signal(SIGTSTP , ctrlZHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }

    //TODO: setup sig alarm handler

    SmallShell& smash = SmallShell::getInstance();
    while(true) {
        std::cout << smash.getPrompet() << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.cmd_line = cmd_line;
        smash.cmd_vector = storeCommandInVector(cmd_line.c_str());
        if (smash.cmd_vector[0] == "chprompt") {
            if (smash.cmd_vector[1].empty()) {
                smash.changePrompt("smash");
            } else {
                smash.changePrompt(smash.cmd_vector[1]);
            }
        }
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}
