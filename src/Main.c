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
    } else if (strcmp(argv[1], "proton")) {
      SetupProton();
      goto exit;
    }

    /* The second argument can be the token we need to log into studio. */
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
