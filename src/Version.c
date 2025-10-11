#include <Shared.h>

int8_t GetVersionData(VersionData *Data) {
  MemoryStruct Chunk;
  Chunk.Memory = malloc(1);
  Chunk.Size = 0;

  CURLcode Response = CurlGet(&Chunk, CLIENT_SETTINGS_URL);
  char **DecodedStrings;
  uint32_t NumberOfStrings = 0;

  DecodedStrings = malloc(6 * sizeof(char *));

  if (unlikely(!DecodedStrings))
    Error("[FATAL]: Unable to allocate DecodedStrings buffer in GetVersionData function.", ERR_MEMORY);

  if (unlikely(Response != CURLE_OK)) {
    Error("[ERROR]: GetVersionData failed downloading the client settings. (%s)", ERR_STANDARD | ERR_NOEXIT, curl_easy_strerror(Response));
    free(DecodedStrings);
    return -1;
  }

  for (uint32_t i = 1; i < Chunk.Size; i++) {
    if (Chunk.Memory[i] == '"') {
      if (NumberOfStrings % 2 == 0) {
        i++;

        do { i++; } while (Chunk.Memory[i] != '"');
        NumberOfStrings++;
      } else {
        char *String;
        uint32_t SpaceRequired = 0, Position = i + 1;
        i++;

        /* First find out how much we have to allocate */
        do {
          SpaceRequired++;
          Position++;
        } while (Chunk.Memory[Position] != '"');

        String = malloc((SpaceRequired + 1) * sizeof(char));

        if (unlikely(!String)) 
          Error("[FATAL]: Not enough memory to allocate String space in GetVersionData function.", ERR_MEMORY);

        /* Store the found string. */
        memcpy(String, Chunk.Memory + i, SpaceRequired);
        String[SpaceRequired] = '\0';

        i += SpaceRequired;

        DecodedStrings[NumberOfStrings / 2] = String;
        NumberOfStrings++;
      }
    }
  }

  Data->Version = DecodedStrings[0];
  Data->ClientVersionUpload = DecodedStrings[1];
  Data->BootstrapperVersion = DecodedStrings[2];

  free(DecodedStrings);
  free(Chunk.Memory);

  return 0;
}
