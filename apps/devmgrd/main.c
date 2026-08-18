#include "daemon.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char *program)
{
    (void)fprintf(stderr, "Usage: %s --device /dev/ttyX [--socket PATH]\n", program);
}

int main(int argc, char **argv)
{
    static const struct option options[] = {{"device", required_argument, NULL, 'd'},
                                             {"socket", required_argument, NULL, 's'},
                                             {"help", no_argument, NULL, 'h'},
                                             {NULL, 0, NULL, 0}};
    struct devmgr_daemon_config config = {0};
    char default_socket[128];
    int option;

    (void)snprintf(default_socket, sizeof(default_socket), "/tmp/devmgrd-%lu.sock",
                   (unsigned long)getuid());
    config.socket_path = default_socket;
    while ((option = getopt_long(argc, argv, "d:s:h", options, NULL)) != -1) {
        switch (option) {
        case 'd': config.device_path = optarg; break;
        case 's': config.socket_path = optarg; break;
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 2;
        }
    }
    if (config.device_path == NULL || optind != argc) {
        usage(argv[0]);
        return 2;
    }
    return devmgr_daemon_run(&config) == 0 ? 0 : 1;
}

