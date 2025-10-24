# Studio Technical Documentation
This document aims to gather information related to how studio functions within a WINE environment, which is basically **almost** the same as it'd be within a normal Windows environment.

# FFlags
The default FFlags file is located at `/prefix/drive_c/users/<user>/AppData/Local/Roblox/ClientSettings/StudioAppSettings.json`. Placing a folder within the studio installation with the name of `ClientSettings` which contains a .json file named `ClientSettings` will make studio use that file instead. FFlags file can (or at least, could) be reloaded at runtime as proven by [this project](https://github.com/grh-official/runtime-fflag-editor).

# Registry
## `HKEY_CURRENT_USER/Software/Wine/Credential Manager/`
This is a WINE specific key within the registry which stores security cookies encrypted using RC4 encryption. The encryption key is stored in a subkey under the Credential Manager key (`EncryptionKey`).

## `HKEY_CURRENT_USER/Software/Roblox/`
This key stores a few information about Studio which for whatever reason are not stored within `/AppData/Local/Roblox/`.

# /AppData/Local/Roblox/
This folder which is located under `prefix/drive_c/steamuser` contains many folders and useful files about Studio, such as global settings, default fflags file, installed plugins and many more.
