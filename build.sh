#!/bin/bash
kernel_version=${2}
kernel_name="Noog-CAF"
device_name=${1}
zip_name="$kernel_name-$device_name-$kernel_version.zip"
kernel_dir=$PWD

export CONFIG_FILE_Z2_PLUS="z2_plus_defconfig"
export CONFIG_FILE_Z2_ROW="z2_row_defconfig"
export ARCH="arm64"
export TOOLCHAIN_PATH="${HOME}/kernel/aarch64-linux-android-4.9"
export CROSS_COMPILE=$TOOLCHAIN_PATH/bin/aarch64-linux-android-
export CONFIG_ABS_PATH="arch/${ARCH}/configs/${CONFIG_FILE}"
export sourcedir=$kernel_dir
export objdir_z2_plus=$kernel_dir/out/z2_plus
export objdir_z2_row=$kernel_dir/out/z2_row
export anykernel=$kernel_dir/anykernel

if [ "$device_name" == "z2_row" ]; then
  CONFIG_FILE=$CONFIG_FILE_Z2_ROW
  objdir=$objdir_z2_row
else
  CONFIG_FILE=$CONFIG_FILE_Z2_PLUS
  objdir=$objdir_z2_plus
fi;
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
