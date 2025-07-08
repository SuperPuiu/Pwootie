#include <Shared.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#define STATIC_PATH "/prefix/drive_c/users/steamuser/AppData/Local/Roblox/ClientSettings/StudioAppSettings.json"

int32_t FileDescriptor;
char *ClientSettings = NULL;

int8_t ApplyFFlag(char *EntryName, char *Data) {
  return 0;
}

int8_t CopyFFlags() {
  return 0;
}

int8_t LoadFFlags() {
  uint32_t StaticLen = strlen(STATIC_PATH), HomeLen = strlen(getenv("HOME"));
  uint32_t InstallDirLen = strlen(INSTALL_DIR);

  char *Path = malloc(StaticLen + HomeLen + InstallDirLen + 3);

  memcpy(Path, getenv("HOME"), HomeLen);
  memcpy(Path + HomeLen + 1, INSTALL_DIR, InstallDirLen);
  memcpy(Path + HomeLen + StaticLen + 2, STATIC_PATH, StaticLen + 1);
  Path[HomeLen] = Path[HomeLen + InstallDirLen + 1] = '/';
  
  FileDescriptor = open(Path, O_RDWR);
  struct stat SettingsStat;
  fstat(FileDescriptor, &SettingsStat);

  ClientSettings = mmap(NULL, SettingsStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, FileDescriptor, 0);
  
  free(Path);
  return 0;
}

void ExitFFLags() {
  close(FileDescriptor);
}
