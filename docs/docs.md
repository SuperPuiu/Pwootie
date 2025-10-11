# Pwootie configuration flags
Pwootie has a few configuration variables which you may make use of. Configuration of the bootstrapper can be done **only** through a configuration file found in `~/.robloxstudio/` with the name `PwootieData.txt`. The configuration supports the following options:

`cdn` - A string for the CDN link to be used. Defaults to `https://setup.rbxcdn.com/`. This is used whenever a new version of ROBLOX Studio is installed.

`update_wine` - A boolean (`true` or `false`) which tells the bootstrapper if it should update the Proton installation whenever possible. 

`wine_binary` - A string which tells the bootstrapper where is the `wine64` executable located. This is automatically modified during update unless `update_wine` boolean is false.

`debug` - A boolean (`true` or `false`) which tells the bootstrapper to let WINE throw errors and warnings in the terminal.

`version` - A string used internally by Pwootie. Modification of this string will cause an update on the next launch. Use `forced_version` instead to force a version.

`forced_version` - A string which instructs Pwootie to install a specific version of ROBLOX Studio.

`zips_checksums` - A string used internally by Pwootie to know whenever a package can be copied from disk or should be downloaded from the CDN. Checksums are split by a semicolon.

# Pwootie launch options
Pwootie has a few launch options which can be used whenever starting the program. Launching the program without any option will launch ROBLOX Studio. Launching the program with an unknown option will launch ROBLOX Studio as well. The launch options are the following:

`reinstall <wine/studio>` - This option tells the bootstrapper to reinstall either wine or studio. The `update_wine` variable is ignored when reinstalling wine.

`fflags <apply/read/generate> <options>` - This option tells the bootstrapper to apply a change to or read a specific fastflag. `read` requires one option, that being the **keyword** to find within the fastflags (that means `read` will go through the entire file and output every fastflag which contains the specified keyword). `apply` requirs two options, those being the **exact name of the fastflag** and a **boolean** for the new status. If a non-boolean is needed, you may have to edit the fastflags file on your own (`~/.robloxstudio/<version>/ClientSettings/ClientAppSettings.json`). `generate` will generate the fflags file using whatever is found in the prefix folder. If studio didn't generate the fflags file, then it'll error saying that it couldn't find a file within the prefix.

`user <add> <options>` - The `add` option tells the bootstrapper to add a new user to the registry key. The parameters for the `add` command are the following: `userid`, `name`, `link` (optional) where `userid` is the account's UserId, `name` is the name which will appear in the account switch list (which can be whatever you want) and `link` is a link to the image you want to appear as your profile picture.

`cookie <read/write> <options>` - Unimplemented.

`wine <config/setup>` - This option tells the bootstrapper to launch anything WINE related. The `config` option will launch the `winecfg` executable found in the same folder as the `wine_binary` binary is located. The `setup` option will run the `SetupPrefix()` function.

# Pwootie Troubleshooting
There are a few issues to keep in mind, such as studio flickering after opening and closing a script, studio not starting in general. To be able to run studio with a version that lacks `VirtualProtectFromApp`, you have two options:
1. Extract the `kernelbase.dll` file from a directory similar to `lib/wine/x86_64-windows` from your WINE installation and add it to your own Pwootie WINE folder, considering you already finalised the installation.
2. Build a WINE version which has a patch which adds `VirtualProtectFromApp` function to `kernelbase.dll`. You can compile our fork of wine [which is found here](https://github.com/SuperPuiu/pwootie-wine).

If Vulkan fails to start for whatever reason, [this link might be useful](https://bbs.archlinux.org/viewtopic.php?id=301979). If it's not, open an issue and I'll try to my best to document the issue and to help.

# Pwootie Building
To build from source you need the following libraries: `curl`, `libzip`. Make sure you have them downloaded before attempting to build the project. Once done, use `make release` and you'll get a new executable in the `bin` folder. Use `make` if you want to build a testing version of the bootstrapper.
