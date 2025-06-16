#include <Shared.h>

void DeleteDirectories() {

}

FILE *OpenPwootieFile(char *Mode) {
  uint32_t HomeLength = strlen(getenv("HOME")), InstallDirLength = strlen(INSTALL_DIR);
  uint32_t FileLength = strlen(PWOOTIE_DATA);

  char *Path = malloc((HomeLength + FileLength + InstallDirLength + 3) * sizeof(char));
  FILE *PwootieFile;

  memcpy(Path, getenv("HOME"), HomeLength);
  memcpy(Path + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(Path + HomeLength + InstallDirLength + 2, PWOOTIE_DATA, FileLength);
  Path[HomeLength] = Path[HomeLength + InstallDirLength + 1] = '/';
  Path[HomeLength + InstallDirLength + FileLength + 2] = '\0';

  PwootieFile = fopen(Path, Mode);

  free(Path);
  return PwootieFile;
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

/* Could be useful for testing if the CDN works. */
void GetCDNVersion(MemoryStruct *VersionStruct) {
  CURLcode Response;
  size_t LengthURL = strlen(CDN_URL), LengthVersion = strlen("version");
  char *FullURL = malloc((LengthURL + LengthVersion + 1) * sizeof(char)); /* One extra byte for null terminator. */
  
  if (!FullURL) {
    printf("[FATAL]: Unable to allocate FullURL in GetVersion function.\n");
    exit(EXIT_FAILURE);
  }

  VersionStruct->Memory = malloc(1);
  VersionStruct->Size = 0;
  
  memcpy(FullURL, CDN_URL, LengthURL);
  memcpy(FullURL + LengthURL, "version", LengthVersion);
  FullURL[LengthURL + LengthVersion + 1] = 0;

  Response = CurlGet(VersionStruct, FullURL);

  if (Response != CURLE_OK) { printf("GetVersion failed: %s\n", curl_easy_strerror(Response)); }
  free(FullURL);
}
