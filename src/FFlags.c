#include <Shared.h>

#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <unistd.h>

#include <ctype.h>

#define DEFAULT_SETTINGS_PATH "/prefix/drive_c/users/steamuser/AppData/Local/Roblox/ClientSettings/StudioAppSettings.json"
#define VERSION_SETTINGS_PATH "ClientSettings/ClientAppSettings.json"

int32_t FileDescriptor;
char *ClientSettings = NULL;

/* ApplyFFlag(char*, char*) is used to change the state of a fast flag.
 * @return 0 on success and -1 on failure. */
int8_t ApplyFFlag(char *EntryName, char *Data) {
  unused(EntryName);
  unused(Data);

  return 0;
}

/* CreateFFlags(char*, char*) is used to create a copy of the studio settings found within the prefix folder.
 * OldVersion can be NULL, which will cause CreateFFlags to use the DEFAULT_SETTINGS_PATH variable.
 * @return 0 on success and -1 on failure. */
int8_t CreateFFlags(char *Version, char *OldVersion) {
  FILE *FFlagsFile, *FileDestination;
  char *FFlagsPath;

  if (!OldVersion)
    FFlagsPath = BuildString(5, getenv("HOME"), "/", INSTALL_DIR, "/", DEFAULT_SETTINGS_PATH);
  else
    FFlagsPath = BuildString(7, getenv("HOME"), "/", INSTALL_DIR, "/", OldVersion, "/", VERSION_SETTINGS_PATH);

  char *DestinationPath = BuildString(7, getenv("HOME"), "/", INSTALL_DIR, "/", Version, "/", VERSION_SETTINGS_PATH);
  char *Buffer = NULL;

  uint32_t DIRECTORY_LEN = strlen(DestinationPath) - 22;
  uint32_t FileSize;

  /* We must build the directory path first. */
  char SavedChar = DestinationPath[DIRECTORY_LEN];
  DestinationPath[DIRECTORY_LEN] = '\0';

  if (BuildDirectoryTree(DestinationPath) != 0) {
    Error("[ERROR]: BuildDirectoryPath failed during CreateFFlags call.\n", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  DestinationPath[DIRECTORY_LEN] = SavedChar;

  FFlagsFile = fopen(FFlagsPath, "r");
  FileDestination = fopen(DestinationPath, "w");

  if (!FFlagsFile) {
    Error("[ERROR]: Unable to open FFlagsFile file.", ERR_STANDARD | ERR_NOEXIT);
    Error(FFlagsPath, ERR_STANDARD | ERR_NOEXIT);
    
    /* Try to use the default copy of the fastflags file if we have OldVersion initialized. */
    if (OldVersion) {
      printf("[INFO]: Attempting to load default fastflags file..\n");
      free(FFlagsPath);
      FFlagsPath = BuildString(5, getenv("HOME"), "/", INSTALL_DIR, "/", DEFAULT_SETTINGS_PATH);
      FFlagsFile = fopen(FFlagsPath, "r");
      
      if (!FFlagsFile) {
        Error("[ERROR]: Unable to open the default fastflags file.\n", ERR_STANDARD | ERR_NOEXIT);
        goto error;
      }
    } else {
      goto error;
    }
  } else if (!FileDestination) {
    Error("[ERROR]: Unable to open FileDestination file.", ERR_STANDARD | ERR_NOEXIT);
    Error(DestinationPath, ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  fseek(FFlagsFile, 0, SEEK_END);
  FileSize = ftell(FFlagsFile);
  Buffer = malloc(FileSize * sizeof(char));

  if (!Buffer)
    Error("[FATAL]: Unable to allocate Buffer during CreateFFlags call.\n", ERR_MEMORY);

  fseek(FFlagsFile, 0, SEEK_SET);

  /* TODO: Error checking. */
  fread(Buffer, sizeof(char), FileSize, FFlagsFile);
  fwrite(Buffer, sizeof(char), FileSize, FileDestination);

  free(Buffer);
  free(FFlagsPath);
  free(DestinationPath);

  fclose(FFlagsFile);
  fclose(FileDestination);
  return 0;

error:
  free(FFlagsPath);
  free(DestinationPath);

  if (Buffer)
    free(Buffer);

  if (FFlagsFile)
    fclose(FFlagsFile);
  if (FileDestination)
    fclose(FileDestination);

  return -1;
}

/* ReadFFlag(char*) is used to read to a buffer the state of a fast flag. 
 * Not to be confused with OutputFFlags(char *EntryName) which is used for console outputting.
 * ReadFFlag will return the **first** match, so the EntryName should be the complete name for the flag.
 * @return the fast flag state on success and NULL on failure. */
char *ReadFFlag(char *EntryName) {
  unused(EntryName);
  return NULL;
}

/* OutputFFlags(char*) is used to output to console fflags which contain the same EntryName substring.
 * Not to be confused with ReadFFlag() which returns a string on the first substring found.
 * @return -1 on failure and 0 on success. */
int8_t OutputFFlags(char *EntryName) {
  if (isalpha(EntryName[0]) == 0) {
    printf("[ERROR]: The name must not contain any special characters.\n");
    return -1; /* You know what you did. */
  }

  if (!ClientSettings)
    Error("[FATAL]: ClientSettings file is not open.", ERR_STANDARD);

  char *StrStart = strstr(ClientSettings, EntryName);
  uint32_t EntryLen = strlen(EntryName);

  if (!StrStart) {
    printf("[ERROR]: Unable to find fast flag %s.", EntryName);
    return -1;
  }

  do {
    uint32_t Position = 0;
    while (StrStart[Position] != ':') {
      if (StrStart[Position] != '"')
        printf("%c", StrStart[Position]);
      Position++;
    }
    Position++;

    printf(": ");

    while (StrStart[Position] != ',') {
      printf("%c", StrStart[Position]);
      Position++;
    }

    printf("\n");
    StrStart = strstr(StrStart + EntryLen, EntryName);
  } while(StrStart);

  return 0;
}

/* LoadFFlags(char*) is used to load the FFlags into memory.
 * @return -1 on failure and 0 on success. */
int8_t LoadFFlags(char *Version) {
  struct stat SettingsStat;
  char *Path = BuildString(7, getenv("HOME"), "/", INSTALL_DIR, "/", Version, "/", VERSION_SETTINGS_PATH);

  FileDescriptor = open(Path, O_RDWR);

  if (FileDescriptor == -1) {
    Error("[ERROR]: LoadFFlags failed to initialize FileDescriptor.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  if (fstat(FileDescriptor, &SettingsStat) == -1) {
    Error("[ERROR]: fstat failed during LoadFFlags call.", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  /* TODO: Error handling */
  ClientSettings = mmap(NULL, SettingsStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, FileDescriptor, 0);

  free(Path);
  return 0;

error:
  free(Path);
  return -1;
}

/* ExitFFLags() should be called when the program quits.
 * @return always NULL. */
void ExitFFLags() {
  close(FileDescriptor);
}
