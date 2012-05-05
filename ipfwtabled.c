/*
 * Copyright (c) 2012,
 * Vadym S. Khondar <v.khondar at invisilabs.com>, InvisiLabs.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the InvisiLabs nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <syslog.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <sys/select.h>

#include <signal.h>

#include <sys/sysctl.h>

#include <pwd.h>

#include <time.h>

#include <sys/queue.h>

#include "ipfwtabled.h"
#include "ipfw.h"

#define DEFAULT_SOCK_TYPE SOCK_DGRAM
#define DEFAULT_BACKLOG 10

#define MIN_CLEANUP_INTERVAL 5
#define MAX_CLEANUP_DELAY 60

#define TAILQ_PRINT(head) \
{ autoexpq_entry * ent; STAILQ_FOREACH(ent, &head, qconnector) { syslog(LOG_DEBUG, "QITEM %x\n", ent); } }

struct configuration
{
  char ** bind_addrs;
  int bind_addrs_cnt;
  int sock_type;
  int daemonize;
  time_t * tbl_exp_periods;
} config = { NULL, 0, -1, 0, NULL };

const size_t messagelen = sizeof(struct message);

void usage(char * progname)
{
  char * usage_info = 
    "Usage: ipfwtabled [-b <host>[:<port>][ -b <host>[:<port>] ...]]\n"
"  [-d] [-t|-u] [-e [<tableidx>]:<timeinsec>[-e <tableidx>:<timeinsec> ...]]\n"
"   -b <host>:<port> - bind address\n"
"   -d               - daemonize\n"
"   -t               - use TCP\n"
"   -u               - use UDP\n"
"   -e [<idx>]:<sec> - specify expiry period for entries of table\n"
"                      idx is index of ipfw table\n"
"                      sec is amount of seconds before entry to be purged\n"
"                      if idx is not specified value is set for all tables\n"
"   -h               - print this message\n";
  fprintf(stderr, usage_info, progname);
}

int getsock(int domain, int type, int proto,
    struct sockaddr * addr, socklen_t addrlen,
    char * caddr)
{
  int fd = socket(domain, type, proto);
  if (fd < 0)
  {
    syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
    return -1;
  }
  if (setsockopt(fd, SOL_SOCKET, SO_RCVLOWAT, &messagelen, sizeof(size_t)))
    syslog(LOG_WARNING, "Failed to set socket low watermark: %s", strerror(errno));
  if (domain == AF_INET)
  {
    int ruseaddr = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &ruseaddr, sizeof(ruseaddr)))
      syslog(LOG_WARNING, "Failed to set address reuse on socket: %s", strerror(errno));
  }
  if (bind(fd, addr, addrlen))
  {
    close(fd);
    syslog(LOG_ERR, "Failed to bind to address '%s': %s", caddr, strerror(errno));
    return -2;
  }
  if (config.sock_type == SOCK_STREAM)
  {
    if (listen(fd, DEFAULT_BACKLOG))
    {
      close(fd);
      syslog(LOG_ERR, "Failed to listen for '%s': %s", caddr, strerror(errno));
      return -3;
    }
  }
  return fd;
}

void sighand(int signum)
{
  syslog(LOG_NOTICE, "Caught %i signal.", signum);
}

int main (int argc, char * argv[])
{
  char * ident = basename(argv[0]);

  uint32_t tables_max;
  size_t tables_max_len = sizeof(tables_max);
  if (sysctlbyname("net.inet.ip.fw.tables_max",
        &tables_max, &tables_max_len, NULL, 0) == -1)
  {
#ifdef IPFW_TABLES_MAX
    warn("Failed to get maximum amount of IPFW tables via sysctl");
    tables_max = IPFW_TABLES_MAX;
#else
    errx(EXIT_FAILURE, "Failed to get maximum amount of IPFW tables");
#endif
  }

  openlog(ident, LOG_PID, LOG_DAEMON);

  /* setting signal handlers */
  signal(SIGTERM, sighand);
  signal(SIGINT, sighand);
  signal(SIGHUP, sighand);

  /* processing command-line args */
  int opt;
  while ((opt = getopt(argc, argv, "b:dv:tue:h")) != -1)
  {
    switch (opt)
    {
      case 'b':
        config.bind_addrs = (char **)realloc(config.bind_addrs, ++config.bind_addrs_cnt);
        config.bind_addrs[config.bind_addrs_cnt - 1] = strdup(optarg);
        syslog(LOG_DEBUG, "Configured address: %s", optarg);
        break;
      case 'd':
        config.daemonize = 1;
        syslog(LOG_DEBUG, "Configured to run in background");
        break;
      case 't':
        if (config.sock_type != -1 && config.sock_type != SOCK_STREAM)
          errx(EXIT_FAILURE, "'-t' and '-u' can't be specified simultaneously.");

        config.sock_type = SOCK_STREAM;
        syslog(LOG_DEBUG, "Configured for streaming sockets");
        break;
      case 'u':
        if (config.sock_type != -1 && config.sock_type != SOCK_DGRAM)
          errx(EXIT_FAILURE, "'-t' and '-u' can't be specified simultaneously.");

        config.sock_type = SOCK_DGRAM;
        syslog(LOG_DEBUG, "Configured for datagram sockets");
        break;
      case 'e':
        if (!config.tbl_exp_periods)
          config.tbl_exp_periods = (time_t *)calloc(tables_max, sizeof(time_t));

        char * expiryspec = strdup(optarg);
        char * s_tblidx = strsep(&expiryspec, ":");
        char * s_exp = (expiryspec) ?
          /*    table index specified */ expiryspec :
          /* no table index specified */ s_tblidx;

        time_t i_exp = (time_t)strtol(s_exp, NULL, 10);
        int i_tblidx = -1;
        if (s_tblidx != s_exp)
          i_tblidx = (int)strtol(s_tblidx, NULL, 10);

        if (i_tblidx != -1)
        {
          if (i_tblidx < 0 || i_tblidx > tables_max)
          {
            warnx("Value of table index must lie within [0;%i).", tables_max);
            continue;
          }

          config.tbl_exp_periods[i_tblidx] = i_exp;
          syslog(LOG_DEBUG, "Configured expiry interval for table (%i) is %i seconds",
              i_tblidx, i_exp);
        } else
        {
          int i;
          for (i = 0; i < tables_max; ++i)
            config.tbl_exp_periods[i] = i_exp;
          syslog(LOG_INFO, "Configured expiry interval for all tables is %i seconds",
              i_exp);
        }
        break;
      case 'h':
        usage(ident);
        return EXIT_SUCCESS;
      default:
        usage(ident);
        return EXIT_FAILURE;
    }
  }

  if (config.sock_type < 0)
    config.sock_type = DEFAULT_SOCK_TYPE;

  /* processing specified bind addresses and creating sockets */
  int socks[FD_SETSIZE], socks_cnt = 0;
  memset(socks, -1, sizeof(socks));
  while (config.bind_addrs_cnt--)
  {
    char * addr = config.bind_addrs[config.bind_addrs_cnt];
    if (addr[0] == '/') /* unix domain socket */
    {
      struct sockaddr_un sun;

      bzero(&sun, sizeof(struct sockaddr_un));
      
      unlink(addr);
      strncpy(sun.sun_path, addr, sizeof(sun.sun_path) - 1);
      sun.sun_family = AF_UNIX;

      int fd = getsock(AF_UNIX, config.sock_type, 0,
          (struct sockaddr *)&sun, SUN_LEN(&sun), addr);

      if (fd < 0)
        continue;

      socks[socks_cnt++] = fd;

      syslog(LOG_INFO, "Listening to %s", addr);

    } else /* inet socket */
    {
      char * host = addr;
      char * port = strchr(host, ':');
      if (port)
      {
        *port = '\0';
        port++;
      }

      struct addrinfo hint, * result, * rp;
      bzero(&hint, sizeof(hint));

      hint.ai_family = AF_UNSPEC;
      hint.ai_socktype = config.sock_type;
      hint.ai_flags = AI_PASSIVE;

      int gai_result = getaddrinfo(host, port, &hint, &result);
      if (gai_result)
        fprintf(stderr, "getaddrinfo: %s", gai_strerror(gai_result));
      else
      {
        for (rp = result; rp != NULL; rp = rp->ai_next)
        {
          if (!port)
            ((struct sockaddr_in *)rp->ai_addr)->sin_port = htons(DEFAULT_PORT);

          int fd = getsock(rp->ai_family, rp->ai_socktype, rp->ai_protocol,
              rp->ai_addr, rp->ai_addrlen, addr);

          if (fd < 0)
            continue;

          socks[socks_cnt++] = fd;

          syslog(LOG_INFO, "Listening to %s port %i", addr,
              ntohs(((struct sockaddr_in *)rp->ai_addr)->sin_port));
        }
      }
    }
  }

  if (!socks_cnt) /* failback if no bind address options succeded/specified */
  {
    do
    {
      struct sockaddr_in saddr;
      bzero(&saddr, sizeof(struct sockaddr_in));
      saddr.sin_family = AF_INET;
      saddr.sin_addr.s_addr = htonl(INADDR_ANY);
      saddr.sin_port = htons(DEFAULT_PORT);
      int fd = getsock(AF_INET, config.sock_type, 0,
         (struct sockaddr *)&saddr, sizeof(struct sockaddr_in), "0.0.0.0");

      if (fd < 0)
        continue;

      socks[socks_cnt++] = fd;

      syslog(LOG_INFO, "Listening to any local address on port %i", DEFAULT_PORT);
    } while (0);

    if (!socks_cnt)
      errx(EXIT_FAILURE, "No address to listen. See syslog for more info.");
  }

  if (config.daemonize && daemon(0, 0) < 0)
    err(EXIT_FAILURE, "Failed to fork into background");

  /* initializing structures for autoexpire */
  STAILQ_HEAD(qhead, __autoexpq_entry) autoexpq_head =
    STAILQ_HEAD_INITIALIZER(autoexpq_head);
  struct __autoexpq_entry
  {
    uint8_t table;
    uint8_t mask;
    time_t timestamp;
    uint32_t addr;
    STAILQ_ENTRY(__autoexpq_entry) qconnector;
  };
  typedef struct __autoexpq_entry autoexpq_entry;
  if (config.tbl_exp_periods) /* if any were configured */
    STAILQ_INIT(&autoexpq_head);

  /* serving */
  fd_set monitor, /* currently monitored sockset */
         rds,     /* readset for select */
         srvs;    /* listened to (server) sockets */
  int i, maxfd = -1;
  FD_ZERO(&srvs);
  for (i = 0; i < socks_cnt; ++i)
  {
    FD_SET(socks[i], &srvs);
    if (socks[i] > maxfd) /* computing first arg for select */
      maxfd = socks[i];
  }

  /* 
   * initializing monitored set of sockets which initially
   * contains only bound server sockets
   */
  memcpy(&monitor, &srvs, sizeof(fd_set));

  /* for select wakeups for tables cleaning */
  struct timeval tv;
  struct timeval * cleanup_interval = NULL;
  time_t cleanup_delay = 0;

  for ( ; ; )
  {
    memcpy(&rds, &monitor, sizeof(fd_set));

    syslog(LOG_DEBUG, "MAXFD = %i", maxfd);

    int readysocks = 0;
    if ((readysocks = select(maxfd + 1, &rds, NULL, NULL, cleanup_interval)) < 0)
    {
      syslog(LOG_ERR, "select: %s", strerror(errno));
      break;
    }

    time_t ct = time(NULL);

    if (!readysocks || cleanup_delay > MAX_CLEANUP_DELAY)
    { /* time limit expired */
      syslog(LOG_DEBUG, "Performing tables cleanup");
      autoexpq_entry * entry = STAILQ_FIRST(&autoexpq_head);
      while (entry && 
          entry->timestamp + config.tbl_exp_periods[entry->table] <= ct)
      { /* while there are entries and entry already expired */
        STAILQ_REMOVE_HEAD(&autoexpq_head, qconnector);
        ipfw_tbl_del(entry->table, entry->addr, entry->mask);
        free(entry);
        entry = STAILQ_FIRST(&autoexpq_head);
      }
      syslog(LOG_DEBUG, "Tables cleanup finished (queue is %s)",
          STAILQ_EMPTY(&autoexpq_head) ? "empty" : "not empty");
    }

    for (i = 0; i < socks_cnt && readysocks; ++i)
    {
      int sock = socks[i];
      int sockidx = i;
      if (sock < 0)
        continue;
      syslog(LOG_DEBUG, "Processing socket %i", sock);

      if (FD_ISSET(sock, &rds))
      {
        --readysocks;
        if (config.sock_type == SOCK_STREAM)
        { /* operation on connected socket */
          if (FD_ISSET(sock, &srvs))
          { /* readiness for reading of server socket means new connection */
            syslog(LOG_DEBUG, "Event on server socket");
            if ((sock = accept(sock, NULL, 0)) < 0)
            {
              syslog(LOG_NOTICE, "Failed to accept connection: %s", strerror(errno));
              continue;
            }

            /* add new socket */
            for (sockidx = 0; sockidx < FD_SETSIZE; ++sockidx)
              if (socks[sockidx] < 0)
              {
                socks[sockidx] = sock;
                break;
              }
            if (sockidx == FD_SETSIZE)
            {
              syslog(LOG_ERR, "Too many simultaneous connections");
              continue;
            }
            if (sockidx >= socks_cnt)
              socks_cnt = sockidx + 1;
            if (sock > maxfd)
              maxfd = sock;

            FD_SET(sock, &monitor);
            if (setsockopt(sock, SOL_SOCKET, SO_RCVLOWAT, &messagelen, sizeof(size_t)))
              syslog(LOG_WARNING, "Failed to set socket low watermark: %s",
                  strerror(errno));
          }
        }

        struct message msg;
        struct sockaddr saddr;
        socklen_t saddrlen;
        int read = recvfrom(sock, &msg, messagelen, MSG_DONTWAIT,
                            &saddr, &saddrlen);
        if (read < 0 && errno == EAGAIN)
          continue; /* still no data after connection - do not wait on recv */
        if (read == messagelen)
        {
          if (msg.table >= tables_max)
          {
            syslog(LOG_ERR, "Table id %i exceeds maximum allowed value (%i)",
                   msg.table, tables_max);
            continue;
          }
          if (msg.mask <= 0)
            msg.mask = 32;
          switch (msg.cmd)
          {
            case CMD_ADD:
              ipfw_tbl_add(msg.table, msg.addr, msg.mask);
              if (config.tbl_exp_periods)
              {
                autoexpq_entry * entry =
                  (autoexpq_entry *)calloc(1, sizeof(autoexpq_entry));

                entry->table = msg.table;
                entry->addr = msg.addr;
                entry->mask = msg.mask;
                entry->timestamp = time(NULL);

                STAILQ_INSERT_TAIL(&autoexpq_head, entry, qconnector);
                struct in_addr ia = { entry->addr };
                syslog(LOG_DEBUG, "Inserted expire entry %s/%i at %i",
                    inet_ntoa(ia), entry->mask, entry->timestamp);
              }
              break;
            case CMD_DEL:
              ipfw_tbl_del(msg.table, msg.addr, msg.mask);
              break;
            case CMD_FLUSH:
              ipfw_tbl_flush(msg.table);
              break;
            default:
              syslog(LOG_NOTICE, "Unknown command: %i", msg.cmd);
              break;
          }
        }

        if (config.sock_type == SOCK_STREAM)
        { /* cleanup after processing connection */
          close(sock);
          FD_CLR(sock, &monitor);
          socks[sockidx] = -1;
          if (i == socks_cnt - 1)
            --socks_cnt;
          if (sock == maxfd)
            --maxfd;
          syslog(LOG_DEBUG, "Cleaned up socket %i", sock);
        }
      } /* if (FD_ISSET(sock, &rds)) */
    } /* for (i = 0; i < socks_cnt && readysocks; ++i) */

    if (config.tbl_exp_periods)
    { /* calculate next cleanup_interval value */
      autoexpq_entry * ent = STAILQ_FIRST(&autoexpq_head);

      if (ent)
      {
        time_t closest_exp = ent->timestamp + config.tbl_exp_periods[ent->table] - ct;
        tv.tv_sec = closest_exp > MIN_CLEANUP_INTERVAL ?
          closest_exp : MIN_CLEANUP_INTERVAL;
        cleanup_interval = &tv;
        cleanup_delay = (closest_exp < 0) ? - closest_exp : 0;
        syslog(LOG_DEBUG, "Next table cleanup in %i seconds "
                          "(current delay %i seconds)",
            tv.tv_sec, cleanup_delay);
      } else
      {
        cleanup_interval = NULL;
        syslog(LOG_DEBUG, "Expiration queue is empty!");
      }
    } /* if (config.tbl_exp_periods) */

  } /* for ( ; ; ) */

  syslog(LOG_INFO, "Exiting.");

  closelog();
  return EXIT_SUCCESS;
}
