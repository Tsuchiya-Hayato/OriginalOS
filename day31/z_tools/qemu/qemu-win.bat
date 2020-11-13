@set SDL_VIDEODRIVER=windib
@set QEMU_AUDIO_DRV=none
@set QEMU_AUDIO_LOG_TO_MONITOR=0
qemu-system-x86_64.exe -L . -m 32 -fda fdimage0.bin -device intel-hda,debug=10 -device hda-duplex,debug=10 -display gtk