#!/bin/bash
kernel_version=${1}
kernel_name="Noog-CAF"
device_name="Z2"
zip_name="$kernel_name-$device_name-$kernel_version.zip"
kernel_dir=$PWD

export CONFIG_FILE="z2_plus_defconfig"
export ARCH="arm64"
export KBUILD_BUILD_USER="Lemoncè"
export KBUILD_BUILD_HOST="DD3Boh"
export TOOLCHAIN_PATH="${HOME}/kernel/aarch64-linux-android-4.9"
export CROSS_COMPILE=$TOOLCHAIN_PATH/bin/aarch64-linux-android-
export CONFIG_ABS_PATH="arch/${ARCH}/configs/${CONFIG_FILE}"
export sourcedir=$kernel_dir
export objdir=$kernel_dir/out
export anykernel=$kernel_dir/anykernel
compile() {
  make O=$objdir  $CONFIG_FILE -j16
  make O=$objdir -j16
}
clean() {
  make O=$objdir CROSS_COMPILE=${CROSS_COMPILE}  $CONFIG_FILE -j16
  make O=$objdir mrproper
}
module_stock(){
  rm -rf $anykernel/modules/
  mkdir $anykernel/modules
  find $objdir -name '*.ko' -exec cp -av {} $anykernel/modules/ \;
  # strip modules
  ${CROSS_COMPILE}strip --strip-unneeded $anykernel/modules/*
  cp -rf $objdir/arch/$ARCH/boot/Image.gz-dtb $anykernel/zImage
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
turn_back(){
cd $sourcedir
}
compile
module_stock
delete_zip
build_package
make_name
turn_back
