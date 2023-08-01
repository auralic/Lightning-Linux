make -j$(nproc) Image.gz 2>&1 | tee build.log
cp arch/arm64/boot/Image.gz ../rootfs/imx8mp-rootfs/
