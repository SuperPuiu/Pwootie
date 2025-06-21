#include <Shared.h>
#include <errno.h>

/* Functions found in Packages.c */
int8_t DownloadPackages(FetchStruct *Fetched, VersionData *Client);
int8_t InstallPackages(FetchStruct *Fetched, VersionData *Client);
FetchStruct *FetchPackages(VersionData *Client);

void DeleteVersion(char *Version) {
  uint32_t InstallDirLength = strlen(INSTALL_DIR), HomeLength = strlen(getenv("HOME"));
  uint32_t VersionLength = strlen(Version);
  uint32_t Total = InstallDirLength + HomeLength + VersionLength + 3;

  char *VersionPath = malloc(Total * sizeof(char));
  
  if (!VersionPath)
    Error("[FATAL]: Unable to allocate VersionPath during DeleteVersion call.", NULL, ERR_MEMORY);

  memcpy(VersionPath, getenv("HOME"), HomeLength);
  memcpy(VersionPath + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(VersionPath + HomeLength + InstallDirLength + 2, Version, VersionLength);
  VersionPath[HomeLength] = VersionPath[HomeLength + InstallDirLength + 1] = '/';
  VersionPath[Total - 1] = '\0';
  
  /* Error checking when? */
  remove(VersionPath);

  free(VersionPath);
}

/* Install() function handles the installation process using the functions found in Packages.c
 * @return -1 on failure and 0 on success. */
int8_t Install(VersionData *Data, uint8_t CheckVersion) {
  /* Before starting, let's check if we already have the right version installed.
   * This process may be skipped by setting CheckVersion flag to 0.*/
  int8_t Status;
  uint8_t Flag = OpenPwootieFile();

  FetchStruct *Fetched;
  char *Version;

  if (CheckVersion == 1 && Flag == 0) {
    Version = PwootieReadEntry("version");

    if (Version) {
      if (strcmp(Version, Data->ClientVersionUpload) == 0) {
        printf("[INFO]: Roblox studio seems to be up-to-date. Aborting installation process.\n");
        free(Version);
        return 0;
      }
    }
  }

  /* First step: get a list of packages we have to download, along with their checksum, real size and compressed size. 
   * FetchStruct also contains some additional information helpful to other functions. */
  Fetched = FetchPackages(Data);
  if (!Fetched)
    return -1;
  
  /* Second step: download the packages using the data we got above. */
  Status = DownloadPackages(Fetched, Data);
  if (Status == -1)
    return -1;

  /* Last step: install the packages. */
  Status = InstallPackages(Fetched, Data);
  if (Status == -1)
    return -1;

  /* Very important: download proton and setup the custom WINE prefix for studio. */
  SetupProton();
  Status = SetupPrefix();
  if (Status == -1)
    return -1;

  /* Write the new version to the version entry of the Pwootie file. 
   * If the flag earlier was set to 0 then it means we have to create the Pwootie file now.*/
  if (!Flag)
    OpenPwootieFile();

  PwootieWriteEntry("version", Data->ClientVersionUpload);
  
  /* Delete the old version, if it exists. */
  if (!Version)
    DeleteVersion(Version);

  /* Clean all the variables and close the PwootieFile. */
  free(Fetched->PackageList);
  free(Fetched);
  free(Version);

  return 0;
}
