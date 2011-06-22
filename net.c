#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "dat.h"
#include "sd-daemon.h"

int
get_socket_from_systemd()
{
    int r, fd;

    /* See if we got a listen fd from systemd. If so, all socket options etc
     * are already set, so we check that the fd is a TCP listen socket and
     * return. */
    r = sd_listen_fds(1);
    if (r < 0) {
        return twarn("sd_listen_fds"), -1;
    }
    if (r > 0) {
        if (r > 1) {
            twarnx("inherited more than one listen socket;"
                   " ignoring all but the first");
            r = 1;
        }
        fd = SD_LISTEN_FDS_START;
        // TODO: handle sd_is_socket_unix
        r = sd_is_socket_inet(fd, 0, SOCK_STREAM, 1, 0);
        if (r < 0) {
            errno = -r;
            twarn("sd_is_socket_inet");
            return -1;
        }
        if (!r) {
            twarnx("inherited fd is not a TCP listen socket");
            return -1;
        }
        return fd;
    }
    return 0;
}

int
make_local_server_socket(char *socket_path)
{
    int fd = -1, r, flags;
    struct sockaddr_un address;

    fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        return twarn("socket()"), -1;

    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;

    // TODO: this won't let someone pass a format string as socket_path,
    // but have I missed some other vulnerability? Bloody C strings!
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", socket_path);

    r = bind(fd, (struct sockaddr *) &(address), sizeof(address));
    if (r == -1)
        return twarn("bind()"), -1;

    // See http://www.tin.org/bin/man.cgi?section=7&topic=AF_LOCAL
    // for more info on socket options (or the lack thereof)

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        twarn("getting flags");
        close(fd);
        return -1;
    }

    r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (r == -1) {
        twarn("setting O_NONBLOCK");
        close(fd);
        return -1;
    }

    r = listen(fd, 1024);
    if (r == -1) {
        twarn("listen()");
        close(fd);
        return -1;
    }

    return fd;
}

int
make_unspec_server_socket(char *host, char *port)
{
    int fd = -1, flags, r;
    struct linger linger = {0, 0};
    struct addrinfo *airoot, *ai, hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    r = getaddrinfo(host, port, &hints, &airoot);
    if (r == -1)
      return twarn("getaddrinfo()"), -1;

    for(ai = airoot; ai; ai = ai->ai_next) {
      fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd == -1) {
        twarn("socket()");
        continue;
      }

      flags = fcntl(fd, F_GETFL, 0);
      if (flags < 0) {
        twarn("getting flags");
        close(fd);
        continue;
      }

      r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
      if (r == -1) {
        twarn("setting O_NONBLOCK");
        close(fd);
        continue;
      }

      flags = 1;
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof flags);
      setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof flags);
      setsockopt(fd, SOL_SOCKET, SO_LINGER,   &linger, sizeof linger);
      setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof flags);

      r = bind(fd, ai->ai_addr, ai->ai_addrlen);
      if (r == -1) {
        twarn("bind()");
        close(fd);
        continue;
      }

      r = listen(fd, 1024);
      if (r == -1) {
        twarn("listen()");
        close(fd);
        continue;
      }

      break;
    }

    freeaddrinfo(airoot);

    if(ai == NULL)
      fd = -1;

    return fd;
}
