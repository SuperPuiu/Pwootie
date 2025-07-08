#define _GNU_SOURCE

#define PREFIX      "prefix"
#define WINEPREFIX  "WINEPREFIX"
#define EXECUTABLE  "RobloxStudioBeta.exe"
#define PROTON_DIR  "proton"
#define PROTON_NAME "wine-proton-10.0-1-amd64.tar.xz"
#define PROTON_LINK "https://github.com/Kron4ek/Wine-Builds/releases/download/proton-10.0-1/wine-proton-10.0-1-amd64.tar.xz"

#include <sys/stat.h>
#include <Shared.h>
#include <errno.h>
#include <pthread.h>

#include <unistd.h>

pthread_t RunThread;

char *GetPrefixPath() {
  uint32_t HomeLength = strlen(getenv("HOME")), InstallDirLength = strlen(INSTALL_DIR);
  uint32_t PrefixLength = strlen(PREFIX);

  char *Location = malloc((HomeLength + InstallDirLength + PrefixLength + 4) * sizeof(char));
  
  if (!Location)
    Error("[FATAL]: Unable to allocate Location buffer during GetPrefixPath call.", ERR_MEMORY);

  memcpy(Location, getenv("HOME"), HomeLength);
  memcpy(Location + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(Location + HomeLength + InstallDirLength + 2, PREFIX, PrefixLength);
  Location[HomeLength] = Location[HomeLength + InstallDirLength + 1] = Location[HomeLength + InstallDirLength + PrefixLength + 2] =  '/';
  Location[HomeLength + InstallDirLength + PrefixLength + 3] = '\0';

  return Location;
}

/* SetupProton() is tasked with downloading and extracting proton.
 * @return -1 on failure and 0 on success. */
int8_t SetupProton() {
  /* Unfortunately for me, the file is a tar.xz, which means that I'd either have to 
   * introduce ANOTHER dependency to Pwootie only for this whole case, or call system().
   * Second choice sounds like the best choice to me. */
  uint32_t HomeLength = strlen(getenv("HOME")), InstallDirLength = strlen(INSTALL_DIR);
  uint32_t ProtonDirLength = strlen(PROTON_DIR), ProtonNameLength = strlen(PROTON_NAME);
  
  CURLcode Response;

  /* 3 for two additional slashes and one additional magic byte.
   * The extra 256 are for building the path to the wine_binary. */
  uint32_t Total = HomeLength + InstallDirLength + ProtonDirLength + ProtonNameLength + 4 + 256;

  char *Path = malloc(Total * sizeof(char)), *PathCopy = malloc(Total * sizeof(char));
  char *Command = malloc(Total * 2 + 3 + 4);
  FILE *TarFile;

  memcpy(Path, getenv("HOME"), HomeLength);
  memcpy(Path + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(Path + HomeLength + InstallDirLength + 2, PROTON_DIR, ProtonDirLength);
  Path[HomeLength] = Path[HomeLength + InstallDirLength + 1] = Path[HomeLength + InstallDirLength + ProtonDirLength + 2] = '/';
  Path[HomeLength + InstallDirLength + ProtonDirLength + 3] = '\0';

  /* First create the directory. */
  if (mkdir(Path, 0755) && errno != EEXIST) {
    Error("[ERROR]: Failed to create the proton installation folder.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  memcpy(PathCopy, Path, Total);

  memcpy(Path + HomeLength + InstallDirLength + ProtonDirLength + 3, PROTON_NAME, ProtonNameLength);
  Path[HomeLength + InstallDirLength + ProtonNameLength + ProtonDirLength + 3] = '\0';

  /* Open file and download it. */
  TarFile = fopen(Path, "w");
  
  if (!TarFile) {
    Error("[ERROR]: Failed to open TarFile which is required to download proton.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  Response = CurlDownload(TarFile, PROTON_LINK);
  // Response = CURLE_OK;

  if (Response != CURLE_OK) {
    Error("[ERROR]: Failed to download the tar file.", ERR_STANDARD | ERR_NOEXIT);
    Error((char*)curl_easy_strerror(Response), ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  fclose(TarFile);

  /* im not going to memcpy this. */
  sprintf(Command, "tar -xvf %s -C %s", Path, PathCopy);
  system(Command);
  
  /* Remove zip file. */
  remove(Path);

  /* Write wine_binary entry. The -6 is used to place the pointer at the start of the extension, after a newly added slash. */
  memcpy(Path + HomeLength + InstallDirLength + ProtonDirLength + ProtonNameLength + 3 - 6, "bin", 3);
  memcpy(Path + HomeLength + InstallDirLength + ProtonDirLength + ProtonNameLength + 1, "wine64", 6);
  Path[HomeLength + InstallDirLength + ProtonDirLength + ProtonNameLength + 3 - 7] = Path[HomeLength + InstallDirLength + ProtonDirLength + ProtonNameLength] = '/';
  Path[HomeLength + InstallDirLength + ProtonDirLength + ProtonNameLength + 7] = '\0';

  PwootieWriteEntry("wine_binary", Path);
  
  /* Cleanup. */
  free(Command);
  free(Path);
  free(PathCopy);
  return 0;

error:
  free(Path);
  free(PathCopy);
  free(Command);
  return -1;
}

/* SetupPrefix() is tasked with setting up the prefix.
 * @return 0 on success, otherwise return -1 on failure. */
int8_t SetupPrefix() {
  int32_t Status;
  /* Is all this string building needed? */
  char *Location = GetPrefixPath();
  
  if (mkdir(Location, 0755) && errno != EEXIST) {
    Error("[FATAL]: Unable to create folder during SetupPrefix call.", ERR_STANDARD | ERR_NOEXIT);
    Error(Location, ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  /* https://www.man7.org/linux/man-pages/man3/setenv.3.html */
  setenv(WINEPREFIX, Location, 1);
  
  /* Install required dlls for studio to launch. Check status values. */
  Status = system("winetricks d3dx11_43");
  if (Status == -1) {
    Error("[FATAL]: winetricks failed to install d3dx11_43. Please try to manually install the component.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  Status = system("winetricks dxvk");
  if (Status == -1) {
    Error("[FATAL]: winetricks failed to install dxvk. Please try to manually install the component.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  /* Studio works only with DPI 98 for whatever reason. 
   * 0x62 is the hexadecimal value of 98. */
  Status = system("wine reg add \"HKEY_CURRENT_USER\\Control Panel\\Desktop\" /v LogPixels /t REG_DWORD /d 0x62 /f");
  if (Status == -1) {
    Error("[FATAL]: Failed to modify prefix registry. Please try to manually change dpi to 98.\n", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  free(Location);
  return 0;

error:
  free(Location);
  return -1;
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
  
  /* Yeah well I can't put it next to the other char arrays. */
  char *WINE_EXEC = PwootieReadEntry("wine_binary");
  uint8_t FreeFlag = 1;

  /* If the wine_binary entry doesn't exist, just create a dummy one and hope wine exists. */
  if (!WINE_EXEC) {
    PwootieWriteEntry("wine_binary", "wine");
    FreeFlag = 0;
    WINE_EXEC = "wine";
  }
  
  uint32_t WineLen = strlen(WINE_EXEC), ArgLen = strlen(Argument);
  uint32_t VersionLen = strlen(Version), HomeLength = strlen(getenv("HOME"));
  uint32_t InstallLen = strlen(INSTALL_DIR), ExecutableLen = strlen(EXECUTABLE);
  uint32_t ExecPathLen = HomeLength + InstallLen + VersionLen + ExecutableLen + 4;
  
  /* Command needs extra space for the quotes required by the arguments. We may also need an additional dot. */
  char *Location = GetPrefixPath();
  char *Command = malloc((WineLen + ArgLen + ExecPathLen + 2 + 3) * sizeof(char));
  char *Executable = malloc(ExecPathLen * sizeof(char));
  
  if (!Command)
    Error("[FATAL]: Unable to allocate Command buffer during Run call.", ERR_MEMORY);
  else if (!Executable)
    Error("[FATAL]: Unable to allocate Executable buffer during Run call.", ERR_MEMORY);

  memcpy(Executable, getenv("HOME"), HomeLength);
  memcpy(Executable + HomeLength + 1, INSTALL_DIR, InstallLen);
  memcpy(Executable + HomeLength + InstallLen + 2, Version, VersionLen);
  memcpy(Executable + HomeLength + InstallLen + VersionLen + 3, EXECUTABLE, ExecutableLen);
  Executable[HomeLength] = Executable[HomeLength + InstallLen + 1] = Executable[HomeLength + InstallLen + VersionLen + 2] = '/';

  memcpy(Command, WINE_EXEC, WineLen);
  memcpy(Command + WineLen + 1, Executable, ExecPathLen);
  memcpy(Command + WineLen + ExecPathLen + 2, Argument, ArgLen);
  Command[WineLen] = Command[WineLen + ExecPathLen] = ' ';
  Command[WineLen + ExecPathLen + 1] = Command[WineLen + ArgLen + ExecPathLen + 2] = '"';
  Command[WineLen + ArgLen + ExecPathLen + 3] = '\0';

  /* Apparently system() can have some security vulnerabilities? eh? */
  setenv(WINEPREFIX, Location, 1);
  pthread_create(&RunThread, NULL, SystemThread, (void*)Command);

  free(Executable);
  free(Location);

  if (FreeFlag)
    free(WINE_EXEC);
}
