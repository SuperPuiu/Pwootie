#include <Shared.h>

#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <unistd.h>

#include <ctype.h>

#define STATIC_PATH           "/prefix/drive_c/users/steamuser/AppData/Local/Roblox/ClientSettings/StudioAppSettings.json"
#define VERSION_SETTINGS_PATH "ClientSettings/ClientAppSettings.json"

int32_t FileDescriptor;
char *ClientSettings = NULL;

int8_t ApplyFFlag(char *EntryName, char *Data) {
  unused(EntryName);
  unused(Data);

  return 0;
}

int8_t CreateFFlags(char *Version) {
  uint32_t HomeLen = strlen(getenv("HOME")), InstallDirLen = strlen(INSTALL_DIR);
  uint32_t StaticPathLen = strlen(STATIC_PATH);
  
  char *FFlagsCopy = malloc(HomeLen + InstallDirLen + StaticPathLen + 3);
  // char *Destination = malloc();
  
  if (!FFlagsCopy)
    Error("[FATAL]: Unable to allocate Path during CreateFFlags call.\n", ERR_MEMORY);
  
  memcpy(FFlagsCopy, getenv("HOME"), HomeLen);
  memcpy(FFlagsCopy + HomeLen + 1, INSTALL_DIR, InstallDirLen);
  memcpy(FFlagsCopy + HomeLen + InstallDirLen + 2, STATIC_PATH, StaticPathLen + 1);
  FFlagsCopy[HomeLen] = FFlagsCopy[HomeLen + InstallDirLen + 1] = '/';
  
  unused(Version);

  return 0;
}

int8_t ReadFFlag(char *EntryName) {
  if (EntryName[0] == '{')
    return -1; /* You know what you did. */

  if (!ClientSettings)
    Error("[FATAL]: Unable to read fflag.\n", ERR_STANDARD);
  
  char *StrStart = strstr(ClientSettings, EntryName);
  int32_t Position = 0;

  if (!StrStart) {
    printf("Unable to find fast flag %s.\n", EntryName);
    return -1;
  }
  
  /* Skip stuff until we reach what we want. */
  while (StrStart[Position - 1] != ':') {
    if (isalpha(StrStart[Position]))
      printf("%c", StrStart[Position]);
    Position++;
  }
  
  printf(" : ");

  /* Read the characters until we reach the comma. */
  while (StrStart[Position] != ',') {
    printf("%c", StrStart[Position]);
    Position++;
  }
  
  printf("\n");

  return 0;
}

int8_t LoadFFlags(char *Version) {
  struct stat SettingsStat;

  uint32_t VersionLen = strlen(Version), HomeLen = strlen(getenv("HOME"));
  uint32_t InstallDirLen = strlen(INSTALL_DIR), SettingsPathLen = strlen(VERSION_SETTINGS_PATH);

  char *Path = malloc(VersionLen + HomeLen + InstallDirLen + SettingsPathLen + 4);
  
  if (!Path)
    Error("[FATAL]: Unable to allocate Path during LoadFFlags call.\n", ERR_MEMORY);

  memcpy(Path, getenv("HOME"), HomeLen);
  memcpy(Path + HomeLen + 1, INSTALL_DIR, InstallDirLen);
  memcpy(Path + HomeLen + InstallDirLen + 2, Version, VersionLen);
  memcpy(Path + HomeLen + InstallDirLen + VersionLen + 3, VERSION_SETTINGS_PATH, SettingsPathLen + 1);
  Path[HomeLen] = Path[HomeLen + InstallDirLen + 1] = Path[HomeLen + InstallDirLen + VersionLen + 2] = '/';

  FileDescriptor = open(Path, O_RDWR);
  
  printf("%s\n", Path);

  if (FileDescriptor == -1) {
    Error("[ERROR]: LoadFFlags failed to initialize FileDescriptor.", ERR_STANDARD | ERR_NOEXIT);
    // CreateFFlags(Version);
    goto error;
  }
  
  if (fstat(FileDescriptor, &SettingsStat) == -1) {
    Error("[ERROR]: fstat failed during LoadFFlags call.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  ClientSettings = mmap(NULL, SettingsStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, FileDescriptor, 0);

  free(Path);
  return 0;

error:
  free(Path);
  return -1;
}

void ExitFFLags() {
  close(FileDescriptor);
}
