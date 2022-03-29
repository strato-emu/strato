<h1 align="center">
    <a href="https://github.com/skyline-emu/skyline" target="_blank">
        <img height="60%" width="60%" src="https://raw.github.com/skyline-emu/branding/master/banner/skyline-banner-rounded.png"><br>
    </a>
    <a href="https://discord.gg/XnbXNQM" target="_blank">
        <img src="https://img.shields.io/discord/545842171459272705.svg?label=&logo=discord&logoColor=ffffff&color=5865F2&labelColor=404EED">
    </a>
    <a href="https://github.com/skyline-emu/skyline/actions/workflows/ci.yml" target="_blank">
        <img src="https://github.com/skyline-emu/skyline/actions/workflows/ci.yml/badge.svg"><br>
    </a>
</h1>

<p align="center">
    <b><a href="CONTRIBUTING.md">Contributing Guide</a> • <a href="BUILDING.md">Building Guide</a></b>
</p>

<p align="center">
    <b>Skyline</b> is an experimental emulator that runs on <b>ARMv8 Android™</b> devices and emulates the functionality of a <b>Nintendo Switch™</b> system, licensed under <a href="https://github.com/skyline-emu/skyline/blob/master/LICENSE.md"><b>Mozilla Public License 2.0</b></a>
</p>

---

### Contact
You can contact the core developers of Skyline at our **[Discord](https://discord.gg/XnbXNQM)**. If you have any questions, feel free to ask. It's also a good place to just keep up with the emulator, as most talk regarding development goes on over there.

---

### Special Thanks
A few noteworthy teams/projects who've helped us along the way are:
* **[Ryujinx](https://ryujinx.org/):** We've used Ryujinx for reference throughout the project, the accuracy of their HLE implementations of Switch subsystems make it an amazing reference. The team behind the project has been extremely helpful with any queries we've had and have constantly helped us with any issues we've come across. **It should be noted that Skyline is not based on Ryujinx**.

* **[yuzu](https://yuzu-emu.org/):** Skyline's shader compiler is a **fork** of *yuzu*'s shader compiler with Skyline-specific changes, using it allowed us to focus on the parts of GPU emulation that we could specifically optimize for mobile while having a high-quality shader compiler implementation as a base. The team behind *yuzu* has also often helped us and have graciously provided us with a license exemption.

* **[Switchbrew](https://github.com/switchbrew/):** We've extensively used Switchbrew whether that be their **[wiki](https://switchbrew.org/)** with its colossal amount of information on the Switch that has saved us countless hours of time or **[libnx](https://github.com/switchbrew/libnx)** which was crucial to initial development of the emulator to ensure that our HLE kernel and sysmodule implementations were accurate.

---

### Disclaimer
* **Nintendo Switch** is a trademark of **Nintendo Co., Ltd**
* **Android** is a trademark of **Google LLC**
