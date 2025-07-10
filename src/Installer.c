#include <Shared.h>
#include <errno.h>

/* Functions found in Packages.c */
int8_t DownloadPackages(FetchStruct *Fetched, char *Version);
int8_t InstallPackages(FetchStruct *Fetched, char *Version);
FetchStruct *FetchPackages(char *Version);

void DeleteVersion(char *Version) {
  uint32_t InstallDirLength = strlen(INSTALL_DIR), HomeLength = strlen(getenv("HOME"));
  uint32_t VersionLength = strlen(Version);
  uint32_t Total = InstallDirLength + HomeLength + VersionLength + 3;

  char *VersionPath = malloc(Total * sizeof(char));
  
  if (!VersionPath)
    Error("[FATAL]: Unable to allocate VersionPath during DeleteVersion call.", ERR_MEMORY);

  memcpy(VersionPath, getenv("HOME"), HomeLength);
  memcpy(VersionPath + HomeLength + 1, INSTALL_DIR, InstallDirLength);
  memcpy(VersionPath + HomeLength + InstallDirLength + 2, Version, VersionLength);
  VersionPath[HomeLength] = VersionPath[HomeLength + InstallDirLength + 1] = '/';
  VersionPath[Total - 1] = '\0';
  
  if (remove(VersionPath) != 0) {
    Error("[ERROR]: Failed to remove old VersionPath.", ERR_STANDARD | ERR_NOEXIT);
    Error(VersionPath, ERR_STANDARD | ERR_NOEXIT);
  }

  free(VersionPath);
}

/* Install() function handles the installation process using the functions found in Packages.c
 * @return -1 on failure and 0 on success. */
int8_t Install(char *Version, uint8_t CheckVersion) {
  printf("[INFO]: Installing %s.\n", Version);

  /* Before starting, let's check if we already have the right version installed.
   * This process may be skipped by setting CheckVersion flag to 0.*/
  int8_t Status = 0;

  FetchStruct *Fetched = NULL;
  char *LastVersion = NULL;

  if (CheckVersion == 1 && PwootieFile) {
    LastVersion = PwootieReadEntry("version");

    if (LastVersion) {
      if (strcmp(LastVersion, Version) == 0) {
        printf("[INFO]: Roblox studio seems to be up-to-date. Aborting installation process.\n");
        free(LastVersion);
        return 0;
      }
    }
  }

  /* First step: get a list of packages we have to download, along with their checksum, real size and compressed size. 
   * FetchStruct also contains some additional information helpful to other functions. */
  Fetched = FetchPackages(Version);
  if (!Fetched)
    goto error;
  
  /* Second step: download the packages using the data we got above. */
  Status = DownloadPackages(Fetched, Version);
  if (Status == -1)
    goto error;

  /* Last step: install the packages. */
  Status = InstallPackages(Fetched, Version);
  if (Status == -1)
    goto error;
  
  /* Write the new version to the version entry of the Pwootie file. 
   * If the flag earlier was set to 0 then it means we have to create the Pwootie file now.*/
  if (!PwootieFile)
    OpenPwootieFile();

  /* Very important: download proton and setup the custom WINE prefix for studio. */
  SetupProton(1);
  Status = SetupPrefix();
  if (Status == -1)
    goto error;

  PwootieWriteEntry("version", Version);
  
  /* Delete the old version, if it exists. */
  if (LastVersion) {
    DeleteVersion(LastVersion);
    free(LastVersion);
  }

  /* Free up the used memory. */
  free(Fetched->PackageList);
  free(Fetched);

  return 0;

error:
  if (Fetched) {
    free(Fetched->PackageList);
    free(Fetched);
  }
  
  if (LastVersion)
    free(LastVersion);

  Error("[ERROR]: An error was encountered while running Install().", ERR_STANDARD | ERR_NOEXIT);

  return -1;
}
