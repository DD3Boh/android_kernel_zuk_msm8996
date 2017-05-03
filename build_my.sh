#!/bin/bash
kernel_version=${1}
kernel_name="rebase-test"
device_name="Z2"
zip_name="$kernel_name-$device_name-$kernel_version.zip"

export CONFIG_FILE="lineageos_z2_plus_defconfig"
export ARCH="arm64"
export CROSS_COMPILE="aarch64-linux-android-"
export KBUILD_BUILD_USER="Lemoncè"
export KBUILD_BUILD_HOST="DD3Boh"
export TOOL_CHAIN_PATH="${HOME}/kernel/aarch64-linux-android-6.x/bin"
export CONFIG_ABS_PATH="arch/${ARCH}/configs/${CONFIG_FILE}"
export PATH=$PATH:${TOOL_CHAIN_PATH}
export objdir="${HOME}/kernel/zuk/obj"
export sourcedir="${HOME}/kernel/zuk/rebase"
export anykernel="${HOME}/kernel/zuk/anykernel"
release_folder="${HOME}/kernel/zuk/releases"
compile() {
  make O=$objdir ARCH=arm64 CROSS_COMPILE=${TOOL_CHAIN_PATH}/${CROSS_COMPILE}  $CONFIG_FILE -j4
  make O=$objdir -j6
}
clean() {
  make O=$objdir ARCH=arm64 CROSS_COMPILE=${TOOL_CHAIN_PATH}/${CROSS_COMPILE}  $CONFIG_FILE -j4
  make O=$objdir mrproper
}
module_stock(){
  rm -rf $anykernel/modules/
  mkdir $anykernel/modules
  find $objdir -name '*.ko' -exec cp -av {} $anykernel/modules/ \;
  # strip modules
 ${TOOL_CHAIN_PATH}/${CROSS_COMPILE}strip --strip-unneeded $anykernel/modules/*
  cp -rf $objdir/arch/arm64/boot/Image.gz-dtb $anykernel/zImage
}
module_cm(){
  rm -rf $anykernel/modules/
  mkdir $anykernel/modules
  find $objdir -name '*.ko' -exec cp -av {} $anykernel/modules/ \;
  # strip modules
 ${TOOL_CHAIN_PATH}/${CROSS_COMPILE}strip --strip-unneeded $anykernel/modules/*
  cp -rf $objdir/arch/arm64/boot/Image.gz-dtb $anykernel/zImage
}
dtbuild(){
  cd $sourcedir
  ./tools/dtbToolCM -2 -o $objdir/arch/arm64/boot/dt.img -s 4096 -p $objdir/scripts/dtc/ $objdir/arch/arm64/boot/dts/
}
delete_zip(){
  cd $anykernel
  find . -name "*.zip" -type f
  find . -name "*.zip" -type f -delete
}
build_package(){
  zip -r9 UPDATE-AnyKernel2.zip * -x README UPDATE-AnyKernel2.zip
}
make_name(){
  mv UPDATE-AnyKernel2.zip $zip_name
}
export_it(){
cp -r $zip_name ../darkmoon-releases/$zip_name
cd $releases_folder
}
push_it(){
cd ..
. push
}
turn_back(){
cd $sourcedir
}
#clean
compile
module_stock
delete_zip
build_package
make_name
#export_it
#push_it
turn_back
#dtbuild
#cp $objdir/arch/arm64/boot/zImage $sourcedir/zImage
#cp $objdir/arch/arm64/boot/dt.img.lz4 $sourcedir/dt.img
