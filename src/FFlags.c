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

static FILE *ClientFile     = NULL;
static char *ClientBuffer   = NULL;
static uint32_t ClientLen   = 0;

/* ApplyFFlag(char*, char*) is used to change the state of a fast flag.
 * @return 0 on success and -1 on failure. */
int8_t ApplyFFlag(char *EntryName, char *Data) {
  if (EntryName == NULL || Data == NULL) {
    printf("[ERROR]: EntryName and Data must not be null when calling ApplyFFlag.\n");
    return -1;
  }
  
  if (unlikely(isalpha(EntryName[0]) == 0 || EntryName[0] == '"')) {
    printf("[ERROR]: The name must not contain any special characters.\n");
    return -1; /* You know what you did. */
  }

  if (unlikely(strcmp(Data, "true") != 0 && strcmp(Data, "false") != 0)) {
    printf("[ERROR]: Data must be either 'true' or 'false'.\n");
    return -1;
  }

  if (!ClientBuffer)
    Error("[FATAL]: ClientSettings file is not open.", ERR_STANDARD);
  
  uint32_t EntryLen = strlen(EntryName);
  uint32_t EntryNameSize = EntryLen * 2;
  
  char *FullEntryName = NULL;
  char *StrStart = strstr(ClientBuffer, EntryName);
  char BooleanData[6];

  if (!StrStart) {
    printf("[ERROR]: Unable to find fast flag %s.", EntryName);
    return -1;
  }

  uint32_t Position = 0, StrStartPosition = ClientLen - strlen(StrStart);
  
  /* Skip first half, aka the entry name. If the name doesn't match with the entry name from the fastflags file,
   * start copying the name and then throw an error. Else continue with the next phase. */
  while (StrStart[Position] != ':') {
    if (unlikely(StrStart[Position] != EntryName[Position] && StrStart[Position] != '"')) {
      FullEntryName = malloc((EntryNameSize + 1) * sizeof(char));
      memcpy(FullEntryName, EntryName, EntryLen);

      while (StrStart[Position] != '"') {
        if (Position > EntryNameSize) {
          char *NewPointer = realloc(FullEntryName, EntryNameSize * 2);

          if (unlikely(!NewPointer))
            Error("Unable to reallocate FullEntryName during ApplyFFlag call.\n", ERR_MEMORY);

          FullEntryName = NewPointer;
        }

        FullEntryName[Position] = StrStart[Position];
        Position++;
      }

      FullEntryName[Position] = '\0';
      goto invalid_entry;
    }

    Position++;
  }

  /* Skip colon and quotation mark. God forbid there's a whitespace. */
  Position += 2;
  StrStartPosition += Position;

  /* Read the fastflag option status, then compare it and check if it's either true or false. */
  for (uint32_t SrcIndex = Position; SrcIndex < Position + 5 && StrStart[SrcIndex] != '"'; SrcIndex++)
    BooleanData[SrcIndex - Position] = StrStart[SrcIndex];

  if (strncmp(BooleanData, "true", 4) == 0) {
    if (strcmp(Data, "true") == 0)
      goto no_change;

    ClientLen += 1;
    char *NewPointer = realloc(ClientBuffer, (ClientLen + 1) * sizeof(char));

    if (!NewPointer)
      Error("[FATAL]: Unable to reallocate ClientBuffer during ApplyFFlag call.\n", ERR_MEMORY);

    ClientBuffer = NewPointer;
    StrStart = ClientBuffer + StrStartPosition;
    
    memmove(StrStart + 5, StrStart + 4, ClientLen - StrStartPosition - 4);
    memcpy(StrStart, "false", 5);
  } else if (strncmp(BooleanData, "false", 5) != 0) {
    if (strcmp(Data, "false") == 0)
      goto no_change;
    
    memmove(StrStart + 4, StrStart + 5, ClientLen - StrStartPosition - 5);
    memcpy(StrStart, "true", 4);
  } else {
    goto non_changeable;
  }

  return 0;
no_change:
  printf("[INFO]: %s is already of state %s.\n", EntryName, Data);
  return 0;
invalid_entry:
  printf("[ERROR]: %s is not an absolute fastflag name. Did you mean \"%s\"?\n", EntryName, FullEntryName);
  free(FullEntryName);
  return -1;
non_changeable:
  printf("[ERROR]: %s is something which can't be changed by Pwootie.\n", EntryName);
  return -1;
}

/* CreateFFlags(char*, char*) is used to create a copy of the studio settings found within the prefix folder.
 * OldVersion can be NULL, which will cause CreateFFlags to use the DEFAULT_SETTINGS_PATH variable.
 * @return 0 on success and -1 on failure. */
int8_t CreateFFlags(char *Version, char *OldVersion) {
  FILE *FFlagsFile = NULL, *FileDestination = NULL;
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
      FFlagsPath = BuildString(4, getenv("HOME"), "/", INSTALL_DIR, DEFAULT_SETTINGS_PATH);
      FFlagsFile = fopen(FFlagsPath, "r");
      
      if (unlikely(!FFlagsFile)) {
        Error("[ERROR]: Unable to open the default fastflags file.\n", ERR_STANDARD | ERR_NOEXIT);
        goto error;
      }
    } else {
      goto error;
    }
  } else if (unlikely(!FileDestination)) {
    Error("[ERROR]: Unable to open FileDestination file.", ERR_STANDARD | ERR_NOEXIT);
    Error(DestinationPath, ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }

  fseek(FFlagsFile, 0, SEEK_END);
  FileSize = ftell(FFlagsFile);
  Buffer = malloc(FileSize * sizeof(char));

  if (unlikely(!Buffer))
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
  if (EntryName == NULL) {
    printf("[ERROR]: EntryName must not be null.\n");
    return NULL;
  }
  return NULL;
}

/* OutputFFlags(char*) is used to output to console fflags which contain the same EntryName substring.
 * Not to be confused with ReadFFlag() which returns a string on the first substring found.
 * @return -1 on failure and 0 on success. */
int8_t OutputFFlags(char *EntryName) {
  if (unlikely(EntryName == NULL)) {
    printf("[ERROR]: EntryName must not be NULL.\n");
    return -1;
  }

  if (unlikely(isalpha(EntryName[0]) == 0)) {
    printf("[ERROR]: The name must not contain any special characters.\n");
    return -1; /* You know what you did. */
  }

  if (!ClientBuffer)
    Error("[FATAL]: ClientBuffer file is not open.", ERR_STANDARD);

  char *StrStart = strstr(ClientBuffer, EntryName);
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
  char *Path = BuildString(7, getenv("HOME"), "/", INSTALL_DIR, "/", Version, "/", VERSION_SETTINGS_PATH);
  ClientFile = fopen(Path, "r+");
  
  if (!ClientFile) {
    Error("[ERROR]: Unable to open ClientFile during LoadFFlags call.\n", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  fseek(ClientFile, 0, SEEK_END);
  ClientLen = ftell(ClientFile);
  fseek(ClientFile, 0, SEEK_SET);
  
  ClientBuffer = malloc((ClientLen + 1) * sizeof(char));
  
  if (!ClientBuffer)
    Error("[ERROR]: Unable to allocate ClientBuffer during LoadFFlags call.\n", ERR_MEMORY);
  
  fread(ClientBuffer, ClientLen, sizeof(char), ClientFile);

  if (ferror(ClientFile) != 0) {
    Error("[ERROR]: Unable to read from ClientFile during LoadFFlags call.\n", ERR_STANDARD | ERR_NOEXIT);
    goto error;
  }
  
  ClientBuffer[ClientLen] = '\0';

  free(Path);
  return 0;

error:
  free(Path);
  return -1;
}

/* ExitFFLags() should be called when the program quits.
 * @return always NULL. */
void ExitFFlags() {
  fseek(ClientFile, 0, SEEK_SET);
  fwrite(ClientBuffer, strlen(ClientBuffer), sizeof(char), ClientFile);
  fclose(ClientFile);
  free(ClientBuffer);
}
