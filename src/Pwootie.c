#include <Shared.h>
#include <errno.h>

FILE *PwootieFile = NULL;
char *PwootieBuffer = NULL;
uint32_t BufferSize = 0;

/* OpenPwootieFile() opens the pwootie file.
 * @return 0 on success and -1 on failure. */
int8_t OpenPwootieFile() {
  /* Check if the file is already open. */
  if (PwootieFile)
    return -1;

  char *Path = BuildString(5, getenv("HOME"), "/", INSTALL_DIR, "/", PWOOTIE_DATA);

  /* Try to open the file in r+ mode, if it fails then try creating it. */
  PwootieFile = fopen(Path, "r+");
  
  if (!PwootieFile) {
    PwootieFile = fopen(Path, "w+");

    /* Maybe creation failed. */
    if (!PwootieFile) {
      Error("Unable to create PwootieFile.", ERR_STANDARD | ERR_NOEXIT);
      free(Path);
      return -1;
    }
  }
  
  /* Allocate the PwootieBuffer. */
  fseek(PwootieFile, 0, SEEK_END);
  
  BufferSize = ftell(PwootieFile);
  PwootieBuffer = malloc(sizeof(char) * BufferSize);

  if (!PwootieBuffer)
    Error("[FATAL]: PwootieBuffer failed to be allocated during OpenPwootieFile.", ERR_MEMORY);

  fseek(PwootieFile, 0, SEEK_SET);
  fread(PwootieBuffer, BufferSize, sizeof(char), PwootieFile);

  /* Cleanup. */
  free(Path);

  return 0;
}

/* PwootieGetEntry()'s whole purpose is to get the start of the entry within the buffer.
 * @return -1 if the entry doesn't exist or a positive number representing the index of where the entry starts in the buffer. */
int32_t PwootieGetEntry(char *Entry) {
  char EntryName[128];

  for (uint32_t SrcIndex = 0; SrcIndex < BufferSize; SrcIndex++) {
    uint8_t EntryIndex = 0;

    do {
      EntryName[EntryIndex] = PwootieBuffer[SrcIndex];
      EntryIndex++;
      SrcIndex++;

      if (EntryIndex == 128)
        Error("[FATAL]: EntryIndex reached number 128. Is the PwooteFile corrupt?", ERR_STANDARD);
      else if (SrcIndex == BufferSize)
        Error("[FATAL]: SrcIndex reached BufferSize. Is the PwootieFile corrupt?", ERR_STANDARD);
    } while (PwootieBuffer[SrcIndex] != '=');
    
    EntryName[EntryIndex] = '\0';

    if (strcmp(EntryName, Entry) == 0)
      return SrcIndex - EntryIndex;

    while (PwootieBuffer[SrcIndex] != '\n')
      SrcIndex++;
  }

  return -1;
}

/* This is the helper function which reads the content of an entry.
 * @return NULL if the entry wasn't added yet.*/
char* PwootieReadEntry(char *Entry) {
  /* Is the PwootieFile open? */
  if (!PwootieFile)
    return NULL;

  uint32_t  DataSize = 1, DataIndex = 0, BufferIndex = 0;
  int32_t   EntryStart = PwootieGetEntry(Entry);
  
  /* No entry to read from. */
  if (EntryStart == -1)
    return NULL;
  
  char *Data = malloc(1 * sizeof(char));

  BufferIndex = (uint32_t)EntryStart;

  while (PwootieBuffer[BufferIndex] != '=')
    BufferIndex++;
  BufferIndex++;

  while (PwootieBuffer[BufferIndex] != '\n') {
    if (DataIndex == DataSize) {
      DataSize *= 2;
      Data = realloc(Data, sizeof(char) * (DataSize + 1));

      if (!Data)
        Error("[FATAL]: Unable to allocate Data during PwootieReadEntry.", ERR_MEMORY);
    }

    Data[DataIndex] = PwootieBuffer[BufferIndex];
    BufferIndex++;
    DataIndex++;
  }

  Data[DataIndex] = '\0';

  return Data;
}

/* PwootieExit() writes the file and closes the file. This is called at the end of the program. */
void PwootieExit() {
  if (!PwootieFile)
    Error("[FATAL]: PwootieFile doesn't exist. Unable to exit properly.", ERR_STANDARD);

  fseek(PwootieFile, 0, SEEK_SET);
  fwrite(PwootieBuffer, BufferSize, sizeof(char), PwootieFile);
  free(PwootieBuffer);
  fclose(PwootieFile);
}

/* PwootieWriteEntry() writes an entry to the buffer. */
void PwootieWriteEntry(char *Entry, char *Data) {
  uint32_t EntrySize = strlen(Entry), DataSize = strlen(Data);
  int32_t EntryIndex = PwootieGetEntry(Entry);
  
  /* If EntryIndex is -1 it means the entry doesn't exist, which also means we have to reallocate to have enough space. */
  if (EntryIndex == -1) {
    EntryIndex = BufferSize;
    PwootieBuffer = realloc(PwootieBuffer, sizeof(char) * (BufferSize + EntrySize + DataSize + 2));
    BufferSize += EntrySize + DataSize + 2;

    if (!PwootieBuffer)
      Error("[FATAL]: Unable to realloc the PwootieBuffer during PwootieWriteEntry.", ERR_MEMORY);
  } else if (BufferSize < EntrySize + DataSize + 2) {
    uint16_t Extra = BufferSize - (EntrySize + DataSize + 2);
    PwootieBuffer = realloc(PwootieBuffer, sizeof(char) * (BufferSize + Extra));
    BufferSize += Extra;

    if (!PwootieBuffer)
      Error("[FATAL]: Unable to realloc the PwootieBuffer during PwootieWriteEntry.", ERR_MEMORY);
  }
  
  /* Write the entry. */
  for (uint32_t Index = 0; Index < EntrySize; Index++) {
    PwootieBuffer[EntryIndex] = Entry[Index];
    EntryIndex++;
  }

  PwootieBuffer[EntryIndex] = '=';
  EntryIndex++;

  for (uint32_t DataIndex = 0; DataIndex < DataSize; DataIndex++) {
    PwootieBuffer[EntryIndex] = Data[DataIndex];
    EntryIndex++;
  }
  
  PwootieBuffer[EntryIndex] = '\n';
}
