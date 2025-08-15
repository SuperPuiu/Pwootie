#include <Shared.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* TODO: https://unix.stackexchange.com/questions/104936/where-are-all-the-posibilities-of-storing-a-log-file */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

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
    printf(ANSI_COLOR_RED "[FATAL]: Unable to allocate Path during Error call. (strerror: %s)\n" ANSI_COLOR_RESET, strerror(errno));
    goto Out;
  }

  memcpy(Path, getenv("HOME"), HomeLength);
  memcpy(Path + HomeLength, LOG_FILE, Locationlength);
  Path[HomeLength + Locationlength] = '\0';

  FILE *Debug = fopen(Path, "a");
  
  if (unlikely(!Debug))
    Debug = fopen(Path, "w");
  
  if (unlikely(!Debug)) {
    printf(ANSI_COLOR_RED "[FATAL]: Unable to open the Pwootie log. (strerror: %s)\n" ANSI_COLOR_RESET, strerror(errno));
    goto Out;
  }

  fprintf(Debug, STANDARD_FORMAT, Time, String);

  if (likely(l_Errno != 0))
    fprintf(Debug, STANDARD_FORMAT, Time, strerror(l_Errno));

Out:
  printf(ANSI_COLOR_RED STANDARD_FORMAT ANSI_COLOR_RESET, Time, String);
  
  if (likely(l_Errno != 0))
    printf(ANSI_COLOR_RED STANDARD_FORMAT ANSI_COLOR_RESET, Time, strerror(l_Errno));
  
  errno = 0;
  free(Path);

  if (~Flags & ERR_NOEXIT || Flags & ERR_MEMORY)
    exit(EXIT_FAILURE);
}
