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
  uint8_t Flag = OpenPwootieFile();
  
  if (CheckVersion == 1 && Flag) {
    char *Version = PwootieReadEntry("version");

    if (Version) {
      if (strcmp(Version, Data->ClientVersionUpload) == 0) {
        printf("[INFO]: Roblox studio seems to be up-to-date. Aborting installation process.\n");
        free(Version);
        return;
      }
    }

    free(Version);
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

  /* Write the new version to the version entry of the Pwootie file. 
   * If the flag earlier was set to 0 then it means we have to create the Pwootie file now.*/
  if (!Flag)
    OpenPwootieFile();

  PwootieWriteEntry("version", Data->ClientVersionUpload);

  /* Clean all the variables and close the PwootieFile. */
  free(Fetched->PackageList);
  free(Fetched);
}
