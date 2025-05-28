// Perf utils with provided code, in .h file for ease of inclusion
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Perf markers
typedef struct {
    int ctl_fd;
    int ack_fd;
    bool enable;
} PerfManager;

const char *enable_cmd = "enable";
const char *disable_cmd = "disable";
const char *ack_cmd = "ack\n";

void send_command(PerfManager *pm, const char *command) {
    if (pm->enable) {
        write(pm->ctl_fd, command, strlen(command));
        char ack[5];
        read(pm->ack_fd, ack, 5);
        assert(strcmp(ack, ack_cmd) == 0);
    }
}

void PerfManager_init(PerfManager *pm) {
    char *ctl_fd_env = getenv("PERF_CTL_FD");
    char *ack_fd_env = getenv("PERF_ACK_FD");
    if (ctl_fd_env && ack_fd_env) {
        pm->enable = true;
        pm->ctl_fd = atoi(ctl_fd_env);
        pm->ack_fd = atoi(ack_fd_env);
    } else {
        pm->enable = false;
        pm->ctl_fd = -1;
        pm->ack_fd = -1;
    }
}

void PerfManager_pause(PerfManager *pm) {
    send_command(pm, disable_cmd);
}
void PerfManager_resume(PerfManager *pm) {
    send_command(pm, enable_cmd);
}
