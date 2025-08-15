#include <Shared.h>

#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>

/* BuildDirectoryTree() builds a path of directories.
 * @return 0 on success and -1 on failure. */
int8_t BuildDirectoryTree(char *Path) {
  /* https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c 
   * Basically, get a pointer to the next separator, replace it with a null byte, create the directory, 
   * then revert the string modification and repeat. Pretty sure this is not thread safe but we don't care. */
  char *Separated = strchr(Path + 1, '/');

  while (Separated != NULL) {
    *Separated = '\0';

    if (unlikely(mkdir(Path, 0755) && errno != EEXIST)) {
      Error("[FATAL]: Mkdir failed during BuildDirectoryTree call.", ERR_STANDARD | ERR_NOEXIT);
      Error(Path, ERR_STANDARD | ERR_NOEXIT);
      return -1;
    }

    *Separated = '/';
    Separated = strchr(Separated + 1, '/');
  }

  return 0;
}

void ReplacePathSlashes(char *Path) {
  uint32_t PathLen = strlen(Path);

  for (uint8_t SrcIndex = 0; SrcIndex < PathLen; SrcIndex++) {
    if (Path[SrcIndex] == 92) {
      Path[SrcIndex] = '/';
      SrcIndex++;
      
      if (Path[SrcIndex] == 92) {
        memmove(Path + SrcIndex, Path + SrcIndex + 1, (PathLen - SrcIndex - 1) * sizeof(char));
        PathLen -= 1;
      }
    }
  }

  /* Hide bugs under the rug because we're good programmers. I don't know why this is required really. */
  Path[PathLen] = '\0';
}

/* BuildString(uint8_t Elements, ...) is usually used for paths, thus it's placement in Filesystem.c.
 * The caller must free the string.
 * @return the built char. */
char *BuildString(uint8_t Elements, ...) {
  va_list Arguments;
  uint64_t Size = 1, LastIndex = 0;
  uint64_t AllSizes[Elements];
  char *Str = NULL;
  
  /* Collect the string lengths. */
  va_start(Arguments, Elements);
  
  for (uint8_t Arg = 0; Arg < Elements; Arg++) {
    uint32_t StrLen = strlen(va_arg(Arguments, char*));
    Size += StrLen;
    AllSizes[Arg] = StrLen;
  }
  
  /* Reset the list and now concatenate the strings. */
  va_start(Arguments, Elements);
  Str = malloc(Size * sizeof(char));
  
  if (unlikely(!Str))
    Error("[FATAL]: Failed to allocate new string memory during BuildString call.\n", ERR_MEMORY);

  for (uint8_t Arg = 0; Arg < Elements; Arg++) {
    memcpy(Str + LastIndex, va_arg(Arguments, char*), AllSizes[Arg]);
    LastIndex += AllSizes[Arg];
  }
  
  Str[Size - 1] = '\0';

  va_end(Arguments);

  return Str;
}

uint64_t QueryDiskSpace() {
  return 0;
}
