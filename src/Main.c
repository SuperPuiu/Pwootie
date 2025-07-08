#include <Shared.h>

int main(int argc, char **argv) {
  OpenPwootieFile();
  curl_global_init(CURL_GLOBAL_ALL);

  VersionData Data;

  SetupHandles();
  GetVersionData(&Data);

  if (argc > 1) {
    if (strcmp(argv[1], "reinstall") == 0) {
      Install(&Data, 0);
      goto exit;
    } else if (strcmp(argv[1], "proton") == 0) {
      SetupProton();
      goto exit;
    } else if (strcmp(argv[1], "fflags") == 0) {
      if (LoadFFlags(Data.ClientVersionUpload) == -1) {
        printf("[INFO]: Failed to load fflags.\n");
        goto exit;
      }
      
      if (argc == 2) {
        printf("[INFO]: fflags require one or more parameters. See the documentation for more information\n");
        goto exit;
      }
      
      if (strcmp(argv[2], "apply") == 0) {
        ApplyFFlag(argv[3], argv[4]);
      } else if (strcmp(argv[2], "read") == 0) {
        ReadFFlag(argv[3]);
      }

      goto exit;
    }

    /* The first argument can be the token we need to log into studio. */
    Run(argv[1], Data.ClientVersionUpload);
  } else {
    Install(&Data, 1);
    Run(NULL, Data.ClientVersionUpload);
  }

exit:
  /* Cleanup */
  free(Data.Version);
  free(Data.ClientVersionUpload);
  free(Data.BootstrapperVersion);

  curl_global_cleanup();
  PwootieExit();
  return 0;
}
