#define _GNU_SOURCE

#include <sys/stat.h>
#include <Shared.h>
#include <errno.h>

#include <unistd.h>

static const char *PREFIX = "prefix";
static const char *WINEPREFIX = "WINEPREFIX";

static char BinPath[PATH_MAX] = {0};

/* Used internally by nftw. */
static int32_t Search(const char *PathName, const struct stat *sbuf, int32_t Type, struct FTW *ftwb) {
  unused(sbuf);
  unused(ftwb);

  if (strstr(PathName, "wine64") && Type == FTW_F) {
    memcpy(BinPath, PathName, strlen(PathName) + 1);
    return 1;
  }

  return 0;
}

/* GetPrefixPath() returns the built prefix path. Allows for ExtraBytes to be allocated by passing a number argument.
 * @return always a char array. */
static char *GetPrefixPath(uint32_t ExtraBytes) {
  uint32_t HomeLength = strlen(getenv("HOME")), InstallDirLength = strlen(INSTALL_DIR);
  uint32_t PrefixLength = strlen(PREFIX);

  char *Location = malloc((HomeLength + InstallDirLength + PrefixLength + ExtraBytes + 4) * sizeof(char));
  
  if (unlikely(!Location))
    Error("[FATAL]: Unable to allocate Location buffer during GetPrefixPath call.", ERR_MEMORY);

  memcpy(Location, getenv("HOME"), HomeLength);
  memcpy(Location + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(Location + HomeLength + InstallDirLength + 2, PREFIX, PrefixLength);
  Location[HomeLength] = Location[HomeLength + InstallDirLength + 1] = Location[HomeLength + InstallDirLength + PrefixLength + 2] =  '/';
  Location[HomeLength + InstallDirLength + PrefixLength + 3] = '\0';

  return Location;
}

/* AddNewUser() adds a new account to the registry, allowing the user to log into it.
 * The registry is HKEY_CURRENT_USER/Software/Roblox/RobloxStudio/LoggedInUserStore/https:/www.roblox.com/users.
 * Format: {"userId1":{"username1":"name","profilePicUrl":"url"}}; {"userId2":{"username":"name","profilePicUrl":"url"}}; ...
 * The format userId should be the account's userid, the name can be anything and the url must be **ANY** valid url to an image. 
 *
 * When first adding the account to the registry key and the user switches to said account, they'll be prompted to use browser login.
 * After that in theory it **should** work. User should close both studio windows after logging into the new account.
 *
 * @return 0 on success and -1 on failure.*/
int8_t AddNewUser(char *UserId, char *Name, char *URL) {
  const char *NEW_USER = "{\\\"%s\\\":{\\\"username\\\":\\\"%s\\\",\\\"profilePicUrl\\\":\\\"%s\\\"}};";

  char *StrtolEndChar = NULL;
  strtol(UserId, &StrtolEndChar, 10);
  
  if (errno != 0 || UserId == NULL || *StrtolEndChar) {
    Error("[ERROR]: UserId is not a valid number.", ERR_STANDARD | ERR_NOEXIT);
    return -1;
  }
  
  URL = URL ? URL : "";
  
  FILE *UserRegistry;
  
  const char *RegistryFileName = "user.reg";
  const char *KeyLocation = "[Software\\\\Roblox\\\\RobloxStudio\\\\LoggedInUsersStore\\\\https:\\\\www.roblox.com]";

  uint32_t UserIdLen = strlen(UserId), NameLen = strlen(Name), URLLen = strlen(URL);
  uint32_t FormatLen = strlen(NEW_USER), RegFileLen = strlen(RegistryFileName);
  uint32_t NewBufferLen = UserIdLen + NameLen + URLLen + FormatLen - 5;
  uint32_t OldKeyLen = 0, RegFileContentSize = 0;

  char *Prefix = GetPrefixPath(RegFileLen + 1);
  char *Buffer = malloc(NewBufferLen);
  char *KeyContent = NULL, *RegFileContent = NULL, *KeyStart = NULL;
  
  if (unlikely(!Buffer))
    Error("[ERROR]: Unable to allocate Buffer during AddNewUser call.", ERR_MEMORY);
  
  sprintf(Buffer, NEW_USER, UserId, Name, URL);
  
  memcpy(Prefix + strlen(Prefix), RegistryFileName, RegFileLen + 1);
  UserRegistry = fopen(Prefix, "r+");

  if (unlikely(!UserRegistry)) {
    Error("[ERROR]: Unable to open registry file.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  fseek(UserRegistry, 0, SEEK_END);
  RegFileContentSize = ftell(UserRegistry);
  fseek(UserRegistry, 0, SEEK_SET);
  
  RegFileContent = malloc((RegFileContentSize + NewBufferLen + 1) * sizeof(char));
  
  if (unlikely(!RegFileContent))
    Error("[ERROR]: Unable to allocate RegFileContent during AddNewUser call.", ERR_MEMORY);
  
  fread(RegFileContent, sizeof(char), RegFileContentSize, UserRegistry);
  RegFileContent[RegFileContentSize] = '\0';

  KeyStart = strstr(RegFileContent, KeyLocation);

  if (unlikely(!KeyStart)) {
    Error("[ERROR]: Unable to find the KeyStart in registry.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  /* Hope we don't hit the wrong key.. */
  KeyStart = strstr(KeyStart, "users");
  if (unlikely(!KeyStart)) {
    Error("[ERROR]: Unable to find users sub-key.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  KeyStart += 7;
  
  do { OldKeyLen++; } while (KeyStart[OldKeyLen] != '\n');
  
  KeyContent = malloc((OldKeyLen + NewBufferLen + 1) * sizeof(char));
  
  if (unlikely(!KeyContent))
    Error("[ERROR]: Unable to allocate KeyContent during AddNewUser call.", ERR_MEMORY);
  
  memcpy(KeyContent, KeyStart, OldKeyLen);
  
  memcpy(KeyContent + OldKeyLen - 1, Buffer, NewBufferLen - 1);
  KeyContent[OldKeyLen + NewBufferLen - 2] = '"';
  KeyContent[OldKeyLen + NewBufferLen - 1] = '\0';
  
  memmove(KeyStart + OldKeyLen + NewBufferLen - 1, KeyStart + OldKeyLen + 1, strlen(KeyStart) - OldKeyLen);
  memcpy(KeyStart, KeyContent, OldKeyLen + NewBufferLen - 1);
  
  fseek(UserRegistry, 0, SEEK_SET);
  fwrite(RegFileContent, sizeof(char), RegFileContentSize + (NewBufferLen - OldKeyLen), UserRegistry);

  fclose(UserRegistry);
  free(KeyContent);
  free(Buffer);
  free(Prefix);
  free(RegFileContent);
  return 0;
error:
  if (UserRegistry)
    fclose(UserRegistry);

  free(Prefix);
  free(Buffer);
  
  if (RegFileContent)
    free(RegFileContent);
  
  if (KeyContent)
    free(KeyContent);

  return -1;
}

/* SetupWine() is tasked with downloading and extracting a default WINE installation.
 * @return -1 on failure and 0 on success. */
int8_t SetupWine(uint8_t CheckExistence) {
  const char *PROTON_NAME = "GE-Proton10-17.tar.gz";
  const char *PROTON_LINK = "https://github.com/GloriousEggroll/proton-ge-custom/releases/download/GE-Proton10-17/GE-Proton10-17.tar.gz";
  const char *PROTON_DIR = "proton";
  char *UpdateWine = PwootieReadEntry("update_wine");
  
  printf("[INFO]: Setting up wine.\n");
  
  if (UpdateWine) {
    if (strcmp(UpdateWine, "false") == 0 && CheckExistence == 1) {
      printf("[INFO]: Option update_wine appears to be false. Aborting installation.\n");
      free(UpdateWine);
      return 0;
    }

    free(UpdateWine);
  }

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
  FILE *TarFile = NULL;

  memcpy(Path, getenv("HOME"), HomeLength);
  memcpy(Path + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(Path + HomeLength + InstallDirLength + 2, PROTON_DIR, ProtonDirLength);
  Path[HomeLength] = Path[HomeLength + InstallDirLength + 1] = Path[HomeLength + InstallDirLength + ProtonDirLength + 2] = '/';
  Path[HomeLength + InstallDirLength + ProtonDirLength + 3] = '\0';

  /* First create the directory. */
  if (mkdir(Path, 0755) && errno != EEXIST) {
    Error("[ERROR]: Failed to create the proton installation folder.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  } else if (errno == EEXIST && CheckExistence == 1) {
    printf("[INFO]: Proton was already installed.\n");
    goto error;
  }
  
  memcpy(PathCopy, Path, Total);

  memcpy(Path + HomeLength + InstallDirLength + ProtonDirLength + 3, PROTON_NAME, ProtonNameLength);
  Path[HomeLength + InstallDirLength + ProtonNameLength + ProtonDirLength + 3] = '\0';

  /* Open file and download it. */
  TarFile = fopen(Path, "w");
  
  if (unlikely(!TarFile)) {
    Error("[ERROR]: Failed to open TarFile which is required to download proton.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  printf("[INFO]: Downloading %s.\n", (char*)PROTON_LINK);
  Response = CurlDownload(TarFile, (char*)PROTON_LINK);
  // Response = CURLE_OK;

  if (unlikely(Response != CURLE_OK)) {
    Error("[ERROR]: Failed to download the tar file.", ERR_STANDARD | ERR_NOEXIT);
    Error((char*)curl_easy_strerror(Response), ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  fclose(TarFile);

  /* im not going to memcpy this. */
  printf("[INFO]: Unzipping new version.\n");
  sprintf(Command, "tar -xvf %s -C %s > /dev/null 2>&1", Path, PathCopy);
  system(Command);
  
  /* Remove zip file. */
  remove(Path);
  
  /* Write wine_binary entry. If wine64 binary can't be found, then throw an error. */
  Path[HomeLength + InstallDirLength + ProtonNameLength + ProtonDirLength - 4] = '\0';
  nftw(Path, Search, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
  
  if (unlikely(BinPath[0] == 0)) {
    Error("[ERROR]: Unable to find wine64 binary. Invalid installation path.", ERR_STANDARD | ERR_NOEXIT);
    Error(Path, ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  printf("%s\n", BinPath);
  PwootieWriteEntry("wine_binary", (char*)BinPath);
  
  /* Cleanup. */
  free(Command);
  free(Path);
  free(PathCopy);

  return 0;

error:
  free(Path);
  free(PathCopy);
  free(Command);

  if (TarFile)
    fclose(TarFile);
  return -1;
}

/* SetupPrefix() is tasked with setting up the prefix.
 * @return 0 on success, otherwise return -1 on failure. */
int8_t SetupPrefix() {
  printf("[INFO]: Setting up prefix.\n");

  int32_t Status;
  /* Is all this string building needed? */
  char *Location = GetPrefixPath(0);
  
  if (mkdir(Location, 0755) && errno != EEXIST) {
    Error("[FATAL]: Unable to create folder during SetupPrefix call.", ERR_STANDARD | ERR_NOEXIT);
    Error(Location, ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  /* https://www.man7.org/linux/man-pages/man3/setenv.3.html */
  setenv(WINEPREFIX, Location, 1);
  
  /* Install required dlls for studio to launch. Check status values. */
  printf("[INFO]: Installing d3dx11_43.\n");
  Status = system("winetricks d3dx11_43 > /dev/null 2>&1");
  if (unlikely(Status != 0)) {
    Error("[FATAL]: winetricks failed to install d3dx11_43. Please try to manually install the component.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  printf("[INFO]: Installing dxvk.\n");
  Status = system("winetricks dxvk > /dev/null 2>&1");
  if (unlikely(Status != 0)) {
    Error("[FATAL]: winetricks failed to install dxvk. Please try to manually install the component.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  free(Location);
  return 0;

error:
  free(Location);
  return -1;
}

void RunWineCfg() {
  uint32_t WineExecLen;

  char *Prefix = GetPrefixPath(0);
  char *WineExec = PwootieReadEntry("wine_binary");
  char *Command;

  if (!WineExec) {
    PwootieWriteEntry("wine_binary", "wine");
    WineExec = malloc((strlen("wine") + 1) * sizeof(char));
    
    if (!WineExec)
      Error("[FATAL]: Unable to allocate memory for WINE_EXEC.", ERR_MEMORY);
  }

  WineExecLen = strlen(WineExec);
  Command = malloc(sizeof(char) * (WineExecLen + 1 + strlen("cfg")));

  if (!Command)
    Error("[FATAL]: Unable to reallocate Command during RunWineCfg call.", ERR_MEMORY);
  
  memcpy(Command, WineExec, WineExecLen);

  for (uint32_t SrcIndex = WineExecLen; SrcIndex > 0; SrcIndex--) {
    if (Command[SrcIndex] != 'e')
      continue;

    memcpy(Command + SrcIndex + 1, "cfg", 4);
    break;
  }

  setenv(WINEPREFIX, Prefix, 1);
  system(Command);

  free(Command);
  free(Prefix);
  free(WineExec);
}

void Run(char *Argument, char *Version) {
  const char *EXECUTABLE ="RobloxStudioBeta.exe";

  if (Argument == NULL)
    Argument = "";
  
  /* Yeah well I can't put them next to the other char arrays. */
  char *WINE_EXEC = PwootieReadEntry("wine_binary");
  char *DEBUG_ENTRY = PwootieReadEntry("debug");
  char *EXTRA_CMD = " > /dev/null 2>&1 & disown";
  uint8_t FreeFlag = 1;

  /* If the wine_binary entry doesn't exist, just create a dummy one and hope wine exists. */
  if (!WINE_EXEC) {
    PwootieWriteEntry("wine_binary", "wine");
    FreeFlag = 0;
    WINE_EXEC = "wine";
  }
  
  if (DEBUG_ENTRY) {
    if (strcmp(DEBUG_ENTRY, "true") == 0)
      EXTRA_CMD = "& disown";
  }

  uint32_t WineLen = strlen(WINE_EXEC), ArgLen = strlen(Argument);
  uint32_t VersionLen = strlen(Version), HomeLength = strlen(getenv("HOME"));
  uint32_t InstallLen = strlen(INSTALL_DIR), ExecutableLen = strlen(EXECUTABLE);
  uint32_t ExtraCmdLen = strlen(EXTRA_CMD);
  uint32_t ExecPathLen = HomeLength + InstallLen + VersionLen + ExecutableLen + 4;
  
  /* Command needs extra space for the quotes required by the arguments. We may also need an additional dot. */
  char *Location = GetPrefixPath(0);
  char *Command = malloc((WineLen + ArgLen + ExecPathLen + ExtraCmdLen + 2 + 3) * sizeof(char));
  char *Executable = malloc(ExecPathLen * sizeof(char));
  
  if (unlikely(!Command))
    Error("[FATAL]: Unable to allocate Command buffer during Run call.", ERR_MEMORY);
  else if (unlikely(!Executable))
    Error("[FATAL]: Unable to allocate Executable buffer during Run call.", ERR_MEMORY);

  memcpy(Executable, getenv("HOME"), HomeLength);
  memcpy(Executable + HomeLength + 1, INSTALL_DIR, InstallLen);
  memcpy(Executable + HomeLength + InstallLen + 2, Version, VersionLen);
  memcpy(Executable + HomeLength + InstallLen + VersionLen + 3, EXECUTABLE, ExecutableLen);
  Executable[HomeLength] = Executable[HomeLength + InstallLen + 1] = Executable[HomeLength + InstallLen + VersionLen + 2] = '/';

  memcpy(Command, WINE_EXEC, WineLen);
  memcpy(Command + WineLen + 1, Executable, ExecPathLen);
  memcpy(Command + WineLen + ExecPathLen + 2, Argument, ArgLen);
  memcpy(Command + WineLen + ArgLen + ExecPathLen + 3, EXTRA_CMD, ExtraCmdLen);
  Command[WineLen] = Command[WineLen + ExecPathLen] = ' ';
  Command[WineLen + ExecPathLen + 1] = Command[WineLen + ArgLen + ExecPathLen + 2] = '"';
  Command[WineLen + ArgLen + ExecPathLen + ExtraCmdLen + 3] = '\0';

  /* Apparently system() can have some security vulnerabilities? eh? */
  setenv(WINEPREFIX, Location, 1);
  system(Command);

  free(Executable);
  free(Location);

  if (FreeFlag)
    free(WINE_EXEC);

  if (DEBUG_ENTRY)
    free(DEBUG_ENTRY);
}
