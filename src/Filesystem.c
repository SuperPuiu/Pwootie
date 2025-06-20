#include <Shared.h>
#include <errno.h>
#include <sys/stat.h>

void BuildDirectoryTree(char *Path) {
  /* https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c 
   * Basically, get a pointer to the next separator, replace it with a null byte, create the directory, 
   * then revert the string modification and repeat. Pretty sure this is not thread safe but we don't care. */
  char *Separated = strchr(Path + 1, '/');

  while (Separated != NULL) {
    *Separated = '\0';

    if (mkdir(Path, 0755) && errno != EEXIST) {
      printf("[FATAL]: Unable to create %s while creating installation directories.\n", Path);
      exit(EXIT_FAILURE);
    }

    *Separated = '/';
    Separated = strchr(Separated + 1, '/');
  }
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
