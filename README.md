# Pwootie
Pwootie is a ROBLOX studio bootstrapper written in C, Linux only compatible. It doesn't do anything better than the other bootstrappers, but it does its job good enough. It should note that it's a barebones bootstrapper, but maybe in the future it'll get be better.

# Building
To build from source you need the following: `curl`, `libzip` and of course `wine`. Make sure you have them downloaded before attempting to build the project.

# Troubleshooting
There are a few issues to keep in mind, such as the fact that cursor locking is not working (at least on the wayland side, if you're using the packaged version of WINE which your distribution has). As a temporary fix for the cursor lock issue, the version of WINE which vinegar uses could be used here as well. If Vulkan fails to start for whatever reason, [this link might be useful](https://bbs.archlinux.org/viewtopic.php?id=301979). If it's not, open an issue and I'll try to my best to document the issue and to help.

# Special thanks
[md5-c](https://github.com/Zunawe/md5-c) for the public domain implementation of the md5 algorithm.

[Cork](https://github.com/CorkHQ/Cork) for the many insights I got on finding out what download links I have to get.

[Vinegar](https://github.com/vinegarhq/vinegar) for the insights on how studio should be set up, and also for other interesting ideas.

[Wine](https://appdb.winehq.org/) for the self explanatory reason.

And many more!

# License
Pwootie is licensed under GPL Version 3.
