# Studio Technical Documentation
This document aims to gather information related to how studio functions within a WINE environment, which is basically **almost** the same as it'd be within a normal Windows environment.

# FFlags
The default FFlags file is located at `/prefix/drive_c/users/<user>/AppData/Local/Roblox/ClientSettings/StudioAppSettings.json`. Placing a folder within the studio installation with the name of `ClientSettings` which contains a .json file named `ClientSettings` will make studio use that file instead. FFlags file can (or at least, could) be reloaded at runtime as proven by [this project](https://github.com/grh-official/runtime-fflag-editor).

# Registry
## `HKEY_CURRENT_USER/Software/Wine/Credential Manager/`
This is a WINE specific key within the registry which stores security cookies encrypted using RC4 encryption. The encryption key is stored in a subkey under the Credential Manager key (`EncryptionKey`).

## `HKEY_CURRENT_USER/Software/Roblox/`
This key stores a few information about Studio which are not stored within `/AppData/Local/Roblox/`. This key includes other keys, those being:
* `<userid>rbxRecentFiles_v03`,
* `BulkImport`,
* `CustomColor`,
* `DraggerGridSize`,
* `emulationDevices`,
* `InfoBarNotifications`,
* `LayoutSettings`,
    - `Docking`
        - `3`
* `LoggedInUserStore`,
    - `https:`
        - `www.roblox.com`
* `OutputWidget`,
    - `MoreOptionSettings`
* `rbxRecentRobloxApiGames_v02_<userid>`,
* `RobloxBetaFeaturesDialog`,
* `RobloxSettingsDialog`,
* `RobloxStudioFirstTimeLoggedIn`,
    - `https:`
        - `www.roblox.com`
* `RobloxStudioLaunchTrackingGuid`,
    - `https:`
        - `www.roblox.com`
* `RobloxStudioMostRecentLogin`,
    - `https:`
        - `www.roblox.com`
* `RPCServers`,
    - `<number>`
* `SplineEditor`,
    - `Transparency`
* `TeachingCallouts`,
* `Themes`

The key naming may or may not be the same for everyone. The key names were imported from my registry.

### `Themes`
The themes key contains only one important subkey, that being, `CurrentTheme`. Aforementioned subkey can have one of the three following states, corresponding with the in-studio settings: `Light`, `Dark` and `Default`. As the name suggests, this subkey controls what theme which is will be used while studio is running.

### `TeachingCallouts`
WIP

### `SplineEditor`
WIP

### `RPCServers`
Appears to be a key containing more keys based on the number of roblox studio instances which are open. Making a local server generates more keys and cleaning up said server will remove the keys associated with it and its clients. The generated keys contains one subkey, that being `ServerName` which is a string in the format `RBX_STUDIO_<number>`.

### `RobloxStudioMostRecentLogin`
Is a key which contains a key named `https:`, which also contains a key named `www.roblox.com`. Last mentioned key contains a subkey named `MostRecentLoginDate` which is in the format `YYYY/MM/DD`.

### `RobloxStudioLaunchTrackingGuid`
This key, like the aformentioned key, contains the same two keys with last key containing one or more keys: `RobloxStudioMachineGuid` and `<userid>` keys.

### `RobloxStudioFirstTimeLoggedIn`
Like `RobloxStudioMostRecentLgin`, this key contains the same two keys, with the last key containing `FirstLoggedInTimestamp` in the format `YYYY-MM-DDTHH:MM:SS:MS` where T is a random thrown in letter, rest characters being self explanatory. Outside of this subkey, it also contains a `REG_SZ` named `LoggedInBefore` which is either `True` or `False`.

### `RobloxSettingsDialog`
The `RobloxSettingsDialog` key contains two `REG_BINARY`: `Geometry` and `SplitterState`.

### `RobloxBetaFeaturesDialog`
The `RobloxBetaFeaturesDialog` key contains one `REG_BINARY` key: `Geometry`.

### `rbxRecentRobloxApiGames_v02_<userid>`

### `OutputWidget`

### `LoggedInUserStore`

### `LayoutSettings`

### `InfoBarNotification`

### `emulationDevices`

### `DraggerGridSize`

### `CustomColor`
This key contains 16 keys, starting from `0` counting to `15`. These keys are of type `REG_BINARY`, and contain information in the following format:
```
0000 40 00 56 00 61 00 72 00 @.V.a.r.
0008 69 00 61 00 6E 00 74 00 i.a.n.t.
0010 28 00 00 00 00 00 00 00 (.......
0018 43 00 01 00 FF 00 FF 00 C...y.y.
0020 C8 00 C8 00 C8 00 C8 00 R.R.G.G.
0028 C8 00 C8 00 00 00 00 00 B.B....
0030 29 00                   ).
```
Where RR is the red color, GG is the green color and BB is the blue color. Rest of the bytes **should** be constant.

### `BulkImport`
This key contains one subkey, that being `LastUsedAssetDirectory` which is a `REG_SZ` containing a path.

### `<userid>rbxRecentFiles_v03`

# /AppData/Local/Roblox/
This folder which is located under `prefix/drive_c/steamuser` contains many folders and useful files about Studio, such as global settings, default fflags file, installed plugins and many more.
