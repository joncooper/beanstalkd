#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include "sd-daemon.h"
#include "dat.h"

static char *user = NULL;
static char *port = "11300";
static char *host_addr;
static char *socket_path = NULL;

static void
su(const char *user) {
    int r;
    struct passwd *pwent;

    errno = 0;
    pwent = getpwnam(user);
    if (errno) twarn("getpwnam(\"%s\")", user), exit(32);
    if (!pwent) twarnx("getpwnam(\"%s\"): no such user", user), exit(33);

    r = setgid(pwent->pw_gid);
    if (r == -1) twarn("setgid(%d \"%s\")", pwent->pw_gid, user), exit(34);

    r = setuid(pwent->pw_uid);
    if (r == -1) twarn("setuid(%d \"%s\")", pwent->pw_uid, user), exit(34);
}


static void
set_sig_handlers()
{
    int r;
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    r = sigemptyset(&sa.sa_mask);
    if (r == -1) twarn("sigemptyset()"), exit(111);

    r = sigaction(SIGPIPE, &sa, 0);
    if (r == -1) twarn("sigaction(SIGPIPE)"), exit(111);

    sa.sa_handler = enter_drain_mode;
    r = sigaction(SIGUSR1, &sa, 0);
    if (r == -1) twarn("sigaction(SIGUSR1)"), exit(111);

    // TODO: figure out if we want to take responsibility for unlinking
    // socket file if we are using one with SASL authentication. Since
    // we would be unable to bind to it in the first place if the file
    // already existed, it would seem that this can't be exploited for
    // evil, but think it through... 
}

static void
usage(char *msg, char *arg)
{
    if (arg) warnx("%s: %s", msg, arg);
    fprintf(stderr, "Use: %s [OPTIONS]\n"
            "\n"
            "Options:\n"
            " -b DIR   wal directory (must be absolute path if used with -d)\n"
            " -f MS    fsync at most once every MS milliseconds"
                       " (use -f 0 for \"always fsync\")\n"
            " -F       never fsync (default)\n"
            " -l ADDR  listen on address (default is 0.0.0.0)\n"
            " -p PORT  listen on port (default is 11300)\n"
            " -u USER  become user and group\n"
            " -z BYTES set the maximum job size in bytes (default is %d)\n"
            " -s BYTES set the size of each wal file (default is %d)\n"
            "            (will be rounded up to a multiple of 512 bytes)\n"
            " -c       compact the binlog (default)\n"
            " -n       do not compact the binlog\n"
            " -A PATH  use SASL authentication via bauthd on socket at PATH\n"
            " -v       show version information\n"
            " -h       show this help\n",
            progname, JOB_DATA_SIZE_LIMIT_DEFAULT, Filesizedef);
    exit(arg ? 5 : 0);
}

static size_t
parse_size_t(char *str)
{
    char r, x;
    size_t size;

    r = sscanf(str, "%zu%c", &size, &x);
    if (1 != r) usage("invalid size", str);
    return size;
}

static char *
require_arg(char *opt, char *arg)
{
    if (!arg) usage("option requires an argument", opt);
    return arg;
}

static void
warn_systemd_ignored_option(char *opt, char *arg)
{
    if (sd_listen_fds(0) > 0) {
        warnx("inherited listen fd; ignoring option: %s %s", opt, arg);
    }
}

static void
opts(int argc, char **argv, Wal *w)
{
    int i;
    int64 ms;

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') usage("unknown option", argv[i]);
        if (argv[i][1] == 0 || argv[i][2] != 0) usage("unknown option",argv[i]);
        switch (argv[i][1]) {
            case 'A':
                socket_path = require_arg("-A", argv[++i]);
                use_local_socket = 1;
                // TODO: if we inherit a PF_UNIX socket from systemd, do we get a path?
                break;
            case 'p':
                port = require_arg("-p", argv[++i]);
                warn_systemd_ignored_option("-p", argv[i]);
                break;
            case 'l':
                host_addr = require_arg("-l", argv[++i]);
                warn_systemd_ignored_option("-l", argv[i]);
                break;
            case 'z':
                job_data_size_limit = parse_size_t(require_arg("-z",
                                                               argv[++i]));
                break;
            case 's':
                w->filesz = parse_size_t(require_arg("-s", argv[++i]));
                break;
            case 'c':
                w->nocomp = 0;
                break;
            case 'n':
                w->nocomp = 1;
                break;
            case 'f':
                ms = (int64)parse_size_t(require_arg("-f", argv[++i]));
                w->syncrate = ms * 1000000;
                w->wantsync = 1;
                break;
            case 'F':
                w->wantsync = 0;
                break;
            case 'u':
                user = require_arg("-u", argv[++i]);
                break;
            case 'b':
                w->dir = require_arg("-b", argv[++i]);
                w->use = 1;
                break;
            case 'h':
                usage(NULL, NULL);
            case 'v':
                printf("beanstalkd %s\n", version);
                exit(0);
            default:
                usage("unknown option", argv[i]);
        }
    }
}

int
make_server_socket(char *host, char *port)
{
    int fd = -1;

    fd = get_socket_from_systemd();
    if (fd)
       return fd;

    if (use_local_socket) {
        fd = make_local_server_socket(socket_path);
    } else {
        fd = make_unspec_server_socket(host, port);
    }

    return fd;
}

int
main(int argc, char **argv)
{
    int r;
    Srv s = {};
    s.wal.filesz = Filesizedef;
    struct job list = {};

    progname = argv[0];
    opts(argc, argv, &s.wal);

    r = make_server_socket(host_addr, port);
    if (r == -1) twarnx("make_server_socket()"), exit(111);
    s.sock.fd = r;

    prot_init();

    if (user) su(user);
    set_sig_handlers();

    if (s.wal.use) {
        // We want to make sure that only one beanstalkd tries
        // to use the wal directory at a time. So acquire a lock
        // now and never release it.
        if (!waldirlock(&s.wal)) {
            twarnx("failed to lock wal dir %s", s.wal.dir);
            exit(10);
        }

        list.prev = list.next = &list;
        walinit(&s.wal, &list);
        prot_replay(&s, &list);
    }

    srv(&s);
    return 0;
}
