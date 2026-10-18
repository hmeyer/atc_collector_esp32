# PlatformIO post-build step: combine bootloader, partition table, boot_app0
# and the app into one image flashed at offset 0 (used by the web flasher).
Import("env")


def merge_bin(source, target, env):
    out = env.subst("$BUILD_DIR/merged.bin")
    parts = []
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        parts += [offset, env.subst(image)]
    parts += [env.subst("$ESP32_APP_OFFSET"), str(target[0])]
    env.Execute(
        " ".join(
            ['"$PYTHONEXE"', '"$OBJCOPY"', "--chip", env.BoardConfig().get("build.mcu"),
             "merge_bin", "-o", f'"{out}"'] + [f'"{p}"' for p in parts]
        )
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
