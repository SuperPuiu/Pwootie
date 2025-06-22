#include <Shared.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* TODO: https://unix.stackexchange.com/questions/104936/where-are-all-the-posibilities-of-storing-a-log-file */
#define LOG_FILE          "/.local/state/PwootieLog.log"
#define STANDARD_FORMAT   "%s | %s\n"
#define EXTENDED_FORMAT   "%s | %s (errno: %s)\n"

/* https://stackoverflow.com/questions/3673226/how-to-print-time-in-format-2009-08-10-181754-811 */
void Error(char *String, char *Additional, uint8_t Flags) {
  /* I don't think we'd have memory to allocate the path for the debug file. */
  if (Flags & ERR_MEMORY)
    goto out;
  
  time_t Timer = time(NULL);
  struct tm *TimeInfo = localtime(&Timer);

  /* Compute path, open the log file and try to write the error to the log file. */
  uint32_t HomeLength = strlen(getenv("HOME")), Locationlength = strlen(LOG_FILE);
  
  char *Path = malloc(sizeof(char) * (HomeLength + Locationlength + 2));
  char TimeBuffer[10];
  
  if (!Path) {
    printf("[FATAL]: No memory to allocate path.");
    exit(EXIT_FAILURE);
  }

  strftime(TimeBuffer, 10, "%H:%M:%S", TimeInfo);

  /* All day and night I'm dreaming memcpy(). */
  memcpy(Path, getenv("HOME"), HomeLength);
  memcpy(Path + HomeLength, LOG_FILE, Locationlength);
  Path[HomeLength + Locationlength] = '\0';

  FILE *Debug = fopen(Path, "a");
  
  if (!Debug)
    Debug = fopen(Path, "w");

  /* Imagine a situation where the Debug file can't be opened at all. */
  if (Debug) {
    fprintf(Debug, Additional != NULL ? EXTENDED_FORMAT : STANDARD_FORMAT, TimeBuffer, String, Additional != NULL ? Additional : "");
    fclose(Debug);
  } else {
    printf("[ERROR]: Unable to open PwootieLog.log. (errno: %s)\n", strerror(errno));
  }
  
  /* Clear errno. */
  errno = 0;

  free(Path);
out:
  printf(Additional != NULL ? EXTENDED_FORMAT : STANDARD_FORMAT, Flags & ERR_MEMORY ? "?" : TimeBuffer, String, Additional != NULL ? Additional : "");
  
  if (~Flags & ERR_NOEXIT || Flags & ERR_MEMORY)
    exit(EXIT_FAILURE);
}
