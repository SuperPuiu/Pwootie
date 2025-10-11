#include <Shared.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

#include <signal.h>

/* TODO: https://unix.stackexchange.com/questions/104936/where-are-all-the-posibilities-of-storing-a-log-file */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define LOG_FILE          "/.local/state/PwootieLog.log"
#define STANDARD_FORMAT   "%s: %s\n"
#define EXTENDED_FORMAT   "%s: %s (strerror: %s)\n"

void Error(char *String, uint8_t Flags, ...) {
  int32_t l_Errno = errno;

  time_t Timestamp = time(NULL);
  va_list Arguments;

  char *Time = ctime(&Timestamp);
  Time[strlen(Time) - 1] = '\0';

  /* Compute path, open the log file and try to write the error to the log file. */
  uint32_t HomeLength = strlen(getenv("HOME")), Locationlength = strlen(LOG_FILE);
  char *Path = malloc(sizeof(char) * (HomeLength + Locationlength + 2));
  char *ChosenFormat = l_Errno != 0 ? EXTENDED_FORMAT : STANDARD_FORMAT;
  char ErrorFormatted[1028];

  va_start(Arguments, Flags);

  if (unlikely(!Path)) {
    printf(ANSI_COLOR_RED "[FATAL]: Unable to allocate Path during Error call. (strerror: %s)\n" ANSI_COLOR_RESET, strerror(errno));
    goto out;
  }
  
  vsprintf(ErrorFormatted, String, Arguments);

  memcpy(Path, getenv("HOME"), HomeLength);
  memcpy(Path + HomeLength, LOG_FILE, Locationlength);
  Path[HomeLength + Locationlength] = '\0';

  FILE *Debug = fopen(Path, "a");

  if (unlikely(!Debug))
    Debug = fopen(Path, "w");

  if (unlikely(!Debug)) {
    printf(ANSI_COLOR_RED "[FATAL]: Unable to open the Pwootie log. (strerror: %s)\n" ANSI_COLOR_RESET, strerror(errno));
    goto out;
  }

  fprintf(Debug, ChosenFormat, Time, ErrorFormatted, l_Errno != 0 ? strerror(l_Errno) : "NULL");
out:
  printf(ANSI_COLOR_RED);
  printf(ChosenFormat, Time, ErrorFormatted, l_Errno != 0 ? strerror(l_Errno) : "NULL");
  printf(ANSI_COLOR_RESET);

  errno = 0;
  free(Path);

  if (~Flags & ERR_NOEXIT || Flags & ERR_MEMORY)
    exit(EXIT_FAILURE);
}

void l_SegFaultHandler(int _, siginfo_t *__, void *___) {
  PwootieExit();

  /* funny */
  unused(_);
  unused(__);
  unused(___);

  exit(EXIT_FAILURE);
}

void SetupSignalHandler() {
  struct sigaction SignalAction;

  memset(&SignalAction, 0, sizeof(sigaction));
  sigemptyset(&SignalAction.sa_mask);

  SignalAction.sa_flags     = SA_NODEFER;
  SignalAction.sa_sigaction = l_SegFaultHandler;

  sigaction(SIGSEGV, &SignalAction, NULL); /* ignore whether it works or not */ 
}
