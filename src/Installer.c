#include <Shared.h>
#include <errno.h>

/* Functions found in Packages.c */
void DownloadPackages(FetchStruct *Fetched, VersionData *Client);
void InstallPackages(FetchStruct *Fetched, VersionData *Client);
FetchStruct *FetchPackages(VersionData *Client);

/* Wrapper for the three beautiful functions: FetchPackages, DownloadPackages and InstallPackages. 
 * This function is SKIPPED if GUID matches the last GUID Pwootie saved. */
void Install(VersionData *Data, uint8_t CheckVersion) {
  /* Before starting, let's check if we already have the right version installed.
   * This process may be skipped by setting CheckVersion flag to 0.*/
  FILE *PwootieFile = OpenPwootieFile("r");
  
  if (CheckVersion == 1) {
    if (PwootieFile) {
      uint32_t VersionLen = strlen(Data->ClientVersionUpload);
      char Version[32];
      
      /* If the file is empty, this returns zero. */
      uint32_t BytesRead = fread(Version, sizeof(char), VersionLen, PwootieFile);

      if (BytesRead != 0) {
        uint8_t SameVersion = 1;
        
        /* Compare the strings. */
        for (uint32_t SrcIndex = 0; SrcIndex < VersionLen; SrcIndex++) {
          if (Data->ClientVersionUpload[SrcIndex] != Version[SrcIndex]) {
            SameVersion = 0;
            break;
          }
        }

        /* We **should** have the same version installed if SameVersion is 1. */
        if (SameVersion == 1) {
          printf("[INFO]: Newest version is already installed. Aborting installation process.\n");
          fclose(PwootieFile);
          return;
        }
      }
    }
  }

  /* First step: get a list of packages we have to download, along with their checksum, real size and compressed size. 
   * FetchStruct also contains some additional information helpful to other functions. */
  FetchStruct *Fetched = FetchPackages(Data);

  /* Second step: download the packages using the data we got above. */
  DownloadPackages(Fetched, Data);

  /* Last step: install the packages. */
  InstallPackages(Fetched, Data);
  
  /* Very important: setup the custom WINE prefix for studio. */
  SetupPrefix();

  /* If the PwootieFile doesn't exist, create one. */
  if (!PwootieFile) {
    PwootieFile = OpenPwootieFile("w+");
    
    /* We should make sure it was actually created. */
    if (!PwootieFile) {
      printf("[FATAL]: Unable to create PwootieFile. (errno: %s)\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  /* Write the new version to the PwootieFile. Add a newline as well, because why not. */
  fwrite(Data->ClientVersionUpload, sizeof(char), strlen(Data->ClientVersionUpload), PwootieFile);
  fwrite("\n", sizeof(char), 1, PwootieFile);

  /* Clean all the variables and close the PwootieFile. */
  free(Fetched->PackageList);
  free(Fetched);

  fclose(PwootieFile);
}
