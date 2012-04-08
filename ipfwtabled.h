#ifndef IPFWTABLED_H
#define IPFWTABLED_H

#define DEFAULT_PORT 12345

#define CMD_ADD   1
#define CMD_DEL   2
#define CMD_FLUSH 3
struct message
{
  uint8_t table;
  char addr[19];
  uint8_t cmd;
};

#endif
