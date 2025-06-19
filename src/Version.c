#include <Shared.h>

void GetVersionData(VersionData *Data) {
  MemoryStruct Chunk;
  Chunk.Memory = malloc(1);
  Chunk.Size = 0;

  CURLcode Response = CurlGet(&Chunk, CLIENT_SETTINGS_URL);
  char **DecodedStrings;
  uint32_t NumberOfStrings = 0;
  
  DecodedStrings = malloc(6 * sizeof(char *));
  
  if (!DecodedStrings) {
    printf("[FATAL]: Unable to allocate DecodedStrings buffer in GetVersionData function.");
    exit(EXIT_FAILURE);
  }

  if (Response != CURLE_OK) {
    printf("[ERROR]: GetVersionData error: %s\n", curl_easy_strerror(Response));
    exit(EXIT_FAILURE);
  } else {
    for (size_t i = 1; i < Chunk.Size; i++) {
      if (Chunk.Memory[i] == '"') {
        char *String;
        uint32_t SpaceRequired = 0, Position = i + 1;
        i++;

        /* First find out how much we have to allocate */
        do {
          SpaceRequired++;
          Position++;
        } while (Chunk.Memory[Position] != '"');
        
        String = calloc(SpaceRequired + 1, sizeof(char));

        if (!String) {
          printf("[FATAL]: Not enough memory to allocate String space in GetVersionData function.\n");
          exit(EXIT_FAILURE);
        }
        
        /* Store the found string. It doesn't have to be NULL byte terminated because calloc sets all bits to 0. */
        memcpy(String, Chunk.Memory + i, SpaceRequired);
        i += SpaceRequired;
        
        DecodedStrings[NumberOfStrings] = String;
        NumberOfStrings++;
      }
    }
  }

  Data->Version = DecodedStrings[1];
  Data->ClientVersionUpload = DecodedStrings[3];
  Data->BootstrapperVersion = DecodedStrings[5];
  
  /* Cleanup. */
  for (size_t i = 0; i < NumberOfStrings; i += 2)
    free(DecodedStrings[i]);
  free(DecodedStrings);

  free(Chunk.Memory);
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
