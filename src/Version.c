#include <Shared.h>

int8_t GetVersionData(VersionData *Data) {
  MemoryStruct Chunk;
  Chunk.Memory = malloc(1);
  Chunk.Size = 0;

  CURLcode Response = CurlGet(&Chunk, CLIENT_SETTINGS_URL);
  char **DecodedStrings;
  uint32_t NumberOfStrings = 0;

  DecodedStrings = malloc(6 * sizeof(char *));

  if (!DecodedStrings)
    Error("[FATAL]: Unable to allocate DecodedStrings buffer in GetVersionData function.", ERR_MEMORY);

  if (Response != CURLE_OK) {
    Error("[ERROR]: GetVersionData failed downloading the client settings.", ERR_STANDARD | ERR_NOEXIT);
    Error((char*)curl_easy_strerror(Response), ERR_STANDARD | ERR_NOEXIT);
    free(DecodedStrings);
    return -1;
  }

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

      String = malloc((SpaceRequired + 1) * sizeof(char));

      if (!String) 
        Error("[FATAL]: Not enough memory to allocate String space in GetVersionData function.", ERR_MEMORY);
      
      /* Store the found string. It doesn't have to be NULL byte terminated because calloc sets all bits to 0. */
      memcpy(String, Chunk.Memory + i, SpaceRequired);
      String[SpaceRequired] = '\0';

      i += SpaceRequired;
      
      DecodedStrings[NumberOfStrings] = String;
      NumberOfStrings++;
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

  return 0;
}

/* Could be useful for testing if the CDN works. */
int8_t GetCDNVersion(MemoryStruct *VersionStruct) {
  CURLcode Response;
  size_t LengthURL = strlen(CDN_URL), LengthVersion = strlen("version");
  char *FullURL = malloc((LengthURL + LengthVersion + 1) * sizeof(char)); /* One extra byte for null terminator. */
  
  if (!FullURL)
    Error("[FATAL]: Unable to allocate FullURL in GetVersion function.\n", ERR_MEMORY);

  VersionStruct->Memory = malloc(1);
  VersionStruct->Size = 0;
  
  memcpy(FullURL, CDN_URL, LengthURL);
  memcpy(FullURL + LengthURL, "version", LengthVersion);
  FullURL[LengthURL + LengthVersion + 1] = 0;

  Response = CurlGet(VersionStruct, FullURL);

  if (Response != CURLE_OK) { 
    Error("[ERROR]: GetCDNVersion failed with curl error.\n", ERR_STANDARD | ERR_NOEXIT);
    Error((char *)curl_easy_strerror(Response), ERR_STANDARD | ERR_NOEXIT);
    free(FullURL);
    return -1; 
  }

  free(FullURL);

  return 0;
}
