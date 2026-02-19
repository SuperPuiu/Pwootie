#include <Shared.h>
#include <RC4.h>

static char *GetEncryptionKey(char *Buffer) {
		const char *KeyLocationFmt = "[Software\\\\Wine\\\\Credential Manager]";

		unused(KeyLocationFmt);
		unused(Buffer);

		return NULL;
}

char *GetCookie(char *UserId) {
		FILE *UserRegistry = GetUserRegistry();
		uint32_t FileSize;

		if (unlikely(!UserRegistry))
				return NULL;

		const char *CookieKeyLocationFmt = "[Software\\\\Wine\\\\Credential Manager\\\\Generic: https://www.roblox.com:RobloxStudioAuth.ROBLOSECURITY%s]";
		char *RegistryBuffer = ReadFileToBuffer(UserRegistry, &FileSize);
		char *EncryptionKey = GetEncryptionKey(RegistryBuffer);
		char *KeyLocation = NULL;
		char *CookieData = NULL;

		uint32_t KeyFormatLen = strlen(CookieKeyLocationFmt);
		uint32_t UserIdLen = strlen(UserId);

		KeyLocation = malloc(UserIdLen + KeyFormatLen + 1);
		RegistryBuffer[FileSize] = 0;

		if (unlikely(!KeyLocation))
				Error("[ERROR]: Unable to malloc KeyLocation during GetCookie.", ERR_MEMORY);

		sprintf(KeyLocation, CookieKeyLocationFmt, UserId);

		free(KeyLocation);
		fclose(UserRegistry);
		return CookieData;
error:
		free(KeyLocation);
		fclose(UserRegistry);
		return NULL;
}

char *SetCookie(char *UserId, char *Cookie) {
		unused(UserId);
		unused(Cookie);

		return NULL;
}

uint8_t UserLogIn(char *UserId, char *Password) {
		unused(UserId);
		unused(Password);

		return 0;
}
