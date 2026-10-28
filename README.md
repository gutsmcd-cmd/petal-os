# Petal OS (はなびらOS)

A small hobby operating system built **from scratch** for Mat. It has no Linux kernel and no code from any other OS: everything (graphics, windows, mouse, keyboard, files, sound, apps) is written in C and x86 assembly in this repo. GRUB is only used to start it.

The colours are muted on purpose: sage, dusty rose and off-white.

## What's inside
- A graphical desktop with picture icons, windows you can drag and resize (pull the bottom-right corner), a taskbar, a clock and the **Petal** start menu
- PS/2 mouse and keyboard (USB mice and keyboards also work on PCs that have "USB legacy support")
- A RAM file system with built-in files. Anything you save is lost when you turn the computer off
- Apps:
  - Files
  - Notes
  - Calculator
  - Calendar with a clock
  - Media Player: plays WAV files through a Sound Blaster 16, tunes through the PC speaker, and shows BMP and PNG pictures (8-bit PNG up to 640 x 480)
  - Paint, which saves to `painting.bmp`
  - Browser: an offline HTML viewer for the built-in pages
  - Settings: English/日本語, desktop colour, clock (computer clock or Japan time, UTC+9)
  - Two games: Snake and Noughts & Crosses
- English and Japanese interface (uses a GNU Unifont subset)

Keyboard: F1–F10 open the apps, F12 opens the menu, Esc closes the front window, and Tab switches windows. Japanese typing (IME) isn't available yet.

## Try it in QEMU
```
qemu-system-i386 -cdrom petal-os.iso -m 256 -rtc base=localtime \
  -audiodev pa,id=snd -machine pcspk-audiodev=snd -device sb16,audiodev=snd
```

## Put it on a USB stick (real PC)
1. Write `petal-os.iso` to the stick with [balenaEtcher](https://etcher.balena.io/) or [Rufus](https://rufus.ie/) (use "DD image" mode). On Linux you can use `sudo dd if=petal-os.iso of=/dev/sdX bs=4M`. This erases the stick.
2. Restart the PC and choose the USB stick from the boot menu (often F12, F11 or Esc).
3. It needs **Legacy/CSM (BIOS) boot**, so turn off Secure Boot and turn on CSM if your PC is UEFI-only. Turn on "USB legacy support" so the keyboard and mouse work.

## Build
On Debian/Ubuntu:
```
sudo apt install gcc gcc-multilib nasm make grub-pc-bin grub-common xorriso mtools python3-pil fonts-unifont
make
make run
```

## Not done yet (future work)
- Online browsing. This needs a network card driver, TCP/IP and HTTPS
- Drivers made for Mat's own PC: USB, the real sound chip, NVMe/SATA disks so files can be saved permanently, Wi-Fi
- UEFI boot, Japanese typing, more file types (MP3, JPEG)

## Credits
- GNU Unifont glyphs (SIL OFL 1.1 / GPL with font exception) for the text
- Everything else is original code in this repo
