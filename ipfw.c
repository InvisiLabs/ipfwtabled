#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>
#include <err.h>
#include <strings.h>
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

  return setsockopt(s, IPPROTO_IP, optname, optval, optlen);
}

void ipfw_tbl_add(int table, in_addr_t addr, u_int8_t mask)
{
  syslog(LOG_DEBUG, "Adding %s/%i to table (%i)", inet_ntoa(addr), mask, table);
  ipfw_table_entry ent;
  int entlen = sizeof(ent);
  bzero(&ent, entlen);
  ent.addr = addr;
  ent.tbl = table;
  ent.masklen = mask;

  if (do_cmd(IP_FW_TABLE_ADD, &ent, entlen) < 0)
    if (errno == EEXIST) /* attempt to delete first */
    {
      if (do_cmd(IP_FW_TABLE_DEL, &ent, entlen) < 0)
        syslog(LOG_ERR, "Add to table failed: %s", strerror(errno));
    } else syslog(LOG_ERR, "Add to table failed: %s", strerror(errno));
}

void ipfw_tbl_del(int table, in_addr_t addr, u_int8_t mask)
{
  syslog(LOG_DEBUG, "Deleting %s/%i to table (%i)", inet_ntoa(addr), mask, table);
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
