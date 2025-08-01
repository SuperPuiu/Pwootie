# Pwootie
Pwootie is a ROBLOX studio bootstrapper written in C, Linux only compatible. It doesn't do anything better than the other bootstrappers, but it does its job good enough. It should note that it's a barebones bootstrapper, but maybe in the future it'll get be better.

# Building
To build from source you need the following: `curl`, `libzip`. Make sure you have them downloaded before attempting to build the project. Once done, use `make release` and you'll get a new executable in the `bin` folder. Use `make` if you want to build a testing version of the bootstrapper.

# Troubleshooting
There are a few issues to keep in mind, such as studio flickering after opening and closing a script, studio not starting in general. To be able to run studio with this bootstrapper, you either have to:
1. Download Vinegar, extract the `kernelbase.dll` file from a directory similar to ` ~/.local/share/flatpak/app/org.vinegarhq.Vinegar/current/5c5c7a02d771ad1b4e2530b5010f39797e27bd0a8732bdb4ec557092c5c22edc/files/lib/wine/x86_64-windows` and add it to your own Pwootie Proton folder, considering you already finalised the installation.
2. Build a WINE version which has a patch which adds `VirtualProtectFromApp` function to `kernelbase.dll`. You can compile our fork of wine [which is found here](https://github.com/SuperPuiu/pwootie-wine).

If Vulkan fails to start for whatever reason, [this link might be useful](https://bbs.archlinux.org/viewtopic.php?id=301979). If it's not, open an issue and I'll try to my best to document the issue and to help.

# Special thanks
[md5-c](https://github.com/Zunawe/md5-c) for the public domain implementation of the md5 algorithm.

[Cork](https://github.com/CorkHQ/Cork) for the many insights I got on finding out what download links I have to get.

[Vinegar](https://github.com/vinegarhq/vinegar) for the insights on how studio should be set up, and also for other interesting ideas.

[Wine](https://appdb.winehq.org/) for the self explanatory reason.

And many more!

# License
Pwootie is licensed under GPL Version 3.
