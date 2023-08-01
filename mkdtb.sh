make DTC_FLAGS="-@" freescale/imx8mp-auralic-aries-dev.dtb
make DTC_FLAGS="-@" freescale/imx8mp-auralic-vega-dev.dtb
cp arch/arm64/boot/dts/freescale/imx8mp-auralic-aries-dev.dtb ../rootfs/imx8mp-rootfs/
cp arch/arm64/boot/dts/freescale/imx8mp-auralic-vega-dev.dtb ../rootfs/imx8mp-rootfs/
