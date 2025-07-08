#include <Shared.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* TODO: https://unix.stackexchange.com/questions/104936/where-are-all-the-posibilities-of-storing-a-log-file */
#define LOG_FILE          "/.local/state/PwootieLog.log"
#define STANDARD_FORMAT   "%s: %s\n"
 
void Error(char *String, uint8_t Flags) {
  int32_t l_Errno = errno;
  time_t Timestamp = time(NULL);
  char *Time = ctime(&Timestamp);
  Time[strlen(Time) - 1] = '\0';

  /* Compute path, open the log file and try to write the error to the log file. */
  uint32_t HomeLength = strlen(getenv("HOME")), Locationlength = strlen(LOG_FILE);
  char *Path = malloc(sizeof(char) * (HomeLength + Locationlength + 2));

  if (!Path) {
    printf("[FATAL]: Unable to allocate Path during Error call. (strerror: %s)\n", strerror(errno));
    goto Out;
  }

  memcpy(Path, getenv("HOME"), HomeLength);
  memcpy(Path + HomeLength, LOG_FILE, Locationlength);
  Path[HomeLength + Locationlength] = '\0';

  FILE *Debug = fopen(Path, "a");
  
  if (!Debug)
    Debug = fopen(Path, "w");
  
  if (!Debug) {
    printf("[FATAL]: Unable to open the Pwootie log. (strerror: %s)\n", strerror(errno));
    goto Out;
  }

  fprintf(Debug, STANDARD_FORMAT, Time, String);

  if (l_Errno != 0)
    fprintf(Debug, STANDARD_FORMAT, Time, strerror(l_Errno));

Out:
  printf(STANDARD_FORMAT, Time, String);
  
  if (l_Errno != 0)
    printf(STANDARD_FORMAT, Time, strerror(l_Errno));
  
  free(Path);

  if (~Flags & ERR_NOEXIT || Flags & ERR_MEMORY)
    exit(EXIT_FAILURE);
}
