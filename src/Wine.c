#define _GNU_SOURCE

#define PREFIX      "prefix"
#define WINEPREFIX  "WINEPREFIX"
#define WINE_EXEC   "wine "
#define EXECUTABLE  "RobloxStudioBeta.exe"

#include <sys/stat.h>
#include <Shared.h>
#include <errno.h>
#include <pthread.h>

pthread_t RunThread;

char *GetPrefixPath() {
  uint32_t HomeLength = strlen(getenv("HOME")), InstallDirLength = strlen(INSTALL_DIR);
  uint32_t PrefixLength = strlen(PREFIX);

  char *Location = malloc((HomeLength + InstallDirLength + PrefixLength + 4) * sizeof(char));
  
  if (!Location) {
    printf("[FATAL]: Unable to allocate Location buffer during SetupPrefix call.\n");
    exit(EXIT_FAILURE);
  }

  memcpy(Location, getenv("HOME"), HomeLength);
  memcpy(Location + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(Location + HomeLength + InstallDirLength + 2, PREFIX, PrefixLength);
  Location[HomeLength] = Location[HomeLength + InstallDirLength + 1] = Location[HomeLength + InstallDirLength + PrefixLength + 2] =  '/';
  Location[HomeLength + InstallDirLength + PrefixLength + 3] = '\0';

  return Location;
}

void SetupPrefix() {
  int32_t Status;
  /* Is all this string building needed? */
  char *Location = GetPrefixPath();
  
  if (mkdir(Location, 0755) && errno != EEXIST) {
    printf("[FATAL]: Unable to create %s during SetupPrefix call.\n", Location);
    exit(EXIT_FAILURE);
  }

  /* https://www.man7.org/linux/man-pages/man3/setenv.3.html */
  setenv(WINEPREFIX, Location, 1);
  
  /* Install required dlls for studio to launch. 
   * TODO: Check status codes. */
  Status = system("winetricks d3dx11_43");
  Status = system("winetricks dxvk");
  
  /* Studio works only with DPI 98 for whatever reason. 
   * 0x62 is the hexadecimal value of 98. */
  Status = system("wine reg add \"HKEY_CURRENT_USER\\Control Panel\\Desktop\" /v LogPixels /t REG_DWORD /d 0x62 /f");
  
  unused(Status);

  free(Location);
}

void *SystemThread(void *Command) {
  char *StrCommand = Command;

  system(StrCommand);

  /* If system() ended then I'm pretty sure we can safely cancel the thread. */
  pthread_cancel(RunThread);
  
  /* It should be obvious why we free it here. */
  free(Command);
  return 0;
}

void Run(char *Argument, char *Version) {
  if (Argument == NULL)
    Argument = "";

  uint32_t WineLen = strlen(WINE_EXEC), ArgLen = strlen(Argument);
  uint32_t VersionLen = strlen(Version), HomeLength = strlen(getenv("HOME"));
  uint32_t InstallLen = strlen(INSTALL_DIR), ExecutableLen = strlen(EXECUTABLE);
  uint32_t ExecPathLen = HomeLength + InstallLen + VersionLen + ExecutableLen + 3;
  
  /* Command needs extra space for the quotes required by the arguments. */
  char *Location = GetPrefixPath();
  char *Command = malloc((WineLen + ArgLen + ExecPathLen + 2 + 2) * sizeof(char));
  char *Executable = malloc((ExecPathLen) * sizeof(char));

  memcpy(Executable, getenv("HOME"), HomeLength);
  memcpy(Executable + HomeLength + 1, INSTALL_DIR, InstallLen);
  memcpy(Executable + HomeLength + InstallLen + 2, Version, VersionLen);
  memcpy(Executable + HomeLength + InstallLen + VersionLen + 3, EXECUTABLE, ExecutableLen);
  Executable[HomeLength] = Executable[HomeLength + InstallLen + 1] = Executable[HomeLength + InstallLen + VersionLen + 2] = '/';

  memcpy(Command, WINE_EXEC, WineLen);
  memcpy(Command + WineLen, Executable, ExecPathLen);
  memcpy(Command + WineLen + ExecPathLen + 2, Argument, ArgLen);
  Command[WineLen + ExecPathLen] = ' ';
  Command[WineLen + ExecPathLen + 1] = Command[WineLen + ArgLen + ExecPathLen + 2] = '"';
  Command[WineLen + ArgLen + ExecPathLen + 3] = '\0';

  /* Apparently system() can have some security vulnerabilities? eh? */
  setenv(WINEPREFIX, Location, 1);
  pthread_create(&RunThread, NULL, SystemThread, (void*)Command);
  
  free(Executable);
  free(Location);
}
