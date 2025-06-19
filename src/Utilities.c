#include <Shared.h>

void DeleteDirectories() {

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
