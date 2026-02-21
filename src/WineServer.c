#include <Shared.h>

static int8_t SetupEnv(char **WineExec, char **Prefix) {
		*Prefix = GetPrefixPath(0);
		*WineExec = PwootieReadEntry("wine_binary", 6); /* "server" is 6 characters. */

		if (unlikely(!WineExec)) {
				*WineExec = GetDefaultWineBinary(6);

				if (unlikely(!(*WineExec)))
						return -1;

				PwootieWriteEntry("wine_binary", *WineExec);
		}

		memcpy((*WineExec) + strlen(*WineExec), "server", 7);
		printf("%s\n", *WineExec);

		setenv("WINEPREFIX", *Prefix, 1);
		return 0;
}

void StartWineserver() {
		char *Prefix = NULL;
		char *Binary = NULL;

		if (unlikely(SetupEnv(&Binary, &Prefix) != -1))
				return;

		ExecProgram(Binary, 0, 1, Binary, "-p", NULL);

		free(Prefix);
		free(Binary);
}

void KillWineserver() {
		char *Prefix = NULL;
		char *Binary = NULL;

		if (unlikely(SetupEnv(&Binary, &Prefix) != -1))
				return;

		free(Prefix);
		free(Binary);
}
