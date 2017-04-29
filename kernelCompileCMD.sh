sudo mount /dev/mmcblk0p1 mount/
sudo cp ./litmus-rt/arch/arm/boot/zImage mount/
sudo cp ./litmus-rt/arch/arm/boot/dts/exynos5422-odroidxu3.dtb mount/
sync


sudo umount mount/
sudo mount /dev/mmcblk0p2 mount/
sudo make modules_install ARCH=arm INSTALL_MOD_PATH=mount/
sync
sudo umount mount/

