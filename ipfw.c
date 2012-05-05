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

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/ip_fw.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>
#include <err.h>
#include <strings.h>
#include <string.h>
#include "ipfw.h"

int do_cmd(int optname, void *optval, uintptr_t optlen)
{
  static int s = -1;

  if (s < 0)
    s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s < 0)
  {
    syslog(LOG_CRIT, "Failed to create socket to contact IPFW: %s", strerror(errno));
    err(EXIT_FAILURE, "Failed to create socket to contact IPFW");
  }

  if (optname == NOOP)
    return 0;

  return setsockopt(s, IPPROTO_IP, optname, optval, optlen);
}

void ipfw_noop(void)
{
  do_cmd(NOOP, NULL, 0);
}

void ipfw_tbl_add(int table, in_addr_t addr, u_int8_t mask)
{
  struct in_addr ia = { addr };
  syslog(LOG_DEBUG, "Adding %s/%i to table (%i)", inet_ntoa(ia), mask, table);
  ipfw_table_entry ent;
  int entlen = sizeof(ent);
  bzero(&ent, entlen);
  ent.addr = addr;
  ent.tbl = table;
  ent.masklen = mask;

  if (do_cmd(IP_FW_TABLE_ADD, &ent, entlen) < 0)
  {
    if (errno == EEXIST) /* attempt to delete first */
    {
      if (do_cmd(IP_FW_TABLE_DEL, &ent, entlen) < 0)
        syslog(LOG_ERR, "Add to table failed: %s", strerror(errno));
      if (do_cmd(IP_FW_TABLE_ADD, &ent, entlen) < 0)
        syslog(LOG_ERR, "Add to table failed: %s", strerror(errno));
    } else syslog(LOG_ERR, "Add to table failed: %s", strerror(errno));
  }
}

void ipfw_tbl_del(int table, in_addr_t addr, u_int8_t mask)
{
  struct in_addr ia = { addr };
  syslog(LOG_DEBUG, "Deleting %s/%i from table (%i)", inet_ntoa(ia), mask, table);
  ipfw_table_entry ent;
  int entlen = sizeof(ent);
  bzero(&ent, entlen);
  ent.addr = addr;
  ent.tbl = table;
  ent.masklen = mask;

  if (do_cmd(IP_FW_TABLE_DEL, &ent, entlen) < 0)
    syslog(LOG_ERR, "Delete from table failed: %s", strerror(errno));
}

void ipfw_tbl_flush(int table)
{
  syslog(LOG_DEBUG, "Flushing table %i", table);
  if (do_cmd(IP_FW_TABLE_FLUSH, &table, sizeof(table)) < 0)
    syslog(LOG_ERR, "Flush table failed: %s", strerror(errno));
}
