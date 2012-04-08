#include <stdio.h>
#include <unistd.h>
#include <libgen.h>

void usage(char * progname)
{
  printf("Usage: ");
  printf("%s [-h <host>:<port>[, -h <host>:<port> ...]] [-d] [-v <verbosity>] [-t|-u]\n",
      basename(progname));
  printf("\t -h <host>:<port> - bind address\n");
  printf("\t -d               - daemonize\n");
  printf("\t -v <verbosity>   - verbosity level (1-6)\n");
  printf("\t -t               - use TCP\n");
  printf("\t -u               - use UDP\n");
  printf("\t -h               - print this message\n");
  printf("\n");
}

/*struct bindaddr*/

struct config
{
};

int main (int argc, char * argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "b:dv:tuh")) != -1)
  {
    switch (opt)
    {
      case 'b':
        printf("Host: %s\n", optarg);
        break;
      case 'd':
        printf("Daemonize\n");
        break;
      case 'v':
        printf("Verbosity: %s\n", optarg);
        break;
      case 't':
        printf("TCP\n");
        break;
      case 'u':
        printf("UDP\n");
        break;
      case 'h':
      default:
        usage(argv[0]);
    }
  }
  return 0;
}
