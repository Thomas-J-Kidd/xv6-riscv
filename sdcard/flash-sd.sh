#!/bin/bash
###########################################
## flash-sd.sh
##
## Written: Jacob Pease jacobpease@protonmail.com
## Created: August 22, 2023
##
## Purpose: A script to flash an sd card with a bootable linux image.
##
## A component of the CORE-V-WALLY configurable RISC-V project.
## https://github.com/openhwgroup/cvw
##
## Copyright (C) 2021-24 Harvey Mudd College & Oklahoma State University
##
## SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
################################################################################################

# Exit on any error (return code != 0)
# set -e

usage() { echo "Usage: $0 [-zh] [-b <path/to/xv6_dir>] <device>" 1>&2; exit 1; }

help() {
    echo "Usage: $0 [OPTIONS] <device>"
    echo "  -z                          wipes card with zeros"
    # Updated description for -b
    echo "  -b <path/to/xv6_dir>      specify path to xv6 build directory (containing kernel/kernel and fs.img)"
    exit 0;
}

# Output colors
GREEN="\e[32m"
RED="\e[31m"
BOLDRED="\e[1;91m"
BOLDGREEN="\e[1;32m"
BOLDYELLOW="\e[1;33m"
NC="\e[0m"
NAME="$BOLDGREEN"${0:2}:"$NC"
ERRORTEXT="$BOLDRED"ERROR:"$NC"
WARNINGTEXT="$BOLDYELLOW"Warning:"$NC"

# Default values for buildroot and device tree
BUILDROOT=$RISCV/buildroot
DEVICE_TREE=wally-vcu108.dtb
MNT_DIR=wallyimg

# Process options and arguments. The following code grabs the single
# sdcard device argument no matter where it is in the positional
# parameters list.
ARGS=()
while [ $OPTIND -le "$#" ] ; do
    if getopts "hzb:d:" arg ; then
        case "${arg}" in
            h) help
               ;;
            z) WIPECARD=y
               ;;
            b) XV6_BUILD_DIR=${OPTARG}  # Store the user-provided path
               ;;
            d) DEVICE_TREE=${OPTARG}
               ;;
        esac
    else
        ARGS+=("${!OPTIND}")
        ((OPTIND++))
    fi
done

# File location variables
# Default XV6_BUILD_DIR if -b is not provided
if [ -z "$XV6_BUILD_DIR" ]; then
    XV6_BUILD_DIR="../../xv6-riscv"
fi

# Resolve the path to be absolute
XV6_BUILD_DIR=$(realpath "$XV6_BUILD_DIR")

XV6_KERNEL="$XV6_BUILD_DIR/kernel/kernel"
XV6_KERNEL_BIN="$XV6_BUILD_DIR/kernel/kernel.bin" #  for raw binary output
XV6_FS_IMG="$XV6_BUILD_DIR/fs.img"

SDCARD=${ARGS[0]}

# Set TOOLPREFIX (if not already set)
if [ -z "$TOOLPREFIX" ]; then
    TOOLPREFIX="riscv64-unknown-elf-" #  Set a default
fi
OBJCOPY="/opt/riscv/bin/${TOOLPREFIX}objcopy" # Construct the full objcopy path

# User Error Checks ===================================================

if [ "$#" -eq "0" ] ; then
    usage
fi

# Check to make sure sd card device exists
if [ ! -e "$SDCARD" ] ; then
    echo -e "$NAME $ERRORTEXT SD card device does not exist."
    exit 1
fi

# Prefix partition with "p" for non-SCSI disks (mmcblk, nvme)
if [[ $SDCARD == "/dev/sd"* ]]; then
    PART_PREFIX=""
else
    PART_PREFIX="p"
fi


# Check for existence of kernel and fs.img
if [ ! -e "$XV6_KERNEL" ] || [ ! -e "$XV6_FS_IMG" ]; then
    echo -e "$NAME $ERRORTEXT Missing images in xv6 build directory: $XV6_BUILD_DIR"
    echo "       Build images before running this script.  Expected files:"
    echo "       Kernel: $XV6_KERNEL"
    echo "       File system: $XV6_FS_IMG"
    echo "       Make sure you have run 'make fs.img' in the xv6-riscv directory."
    exit 1
fi


# Calculate partition information =====================================

# XV6 kernel size
KERNEL_SIZE=$(ls -la --block-size=512 "$XV6_KERNEL" | cut -d' ' -f 5 )
FS_IMG_SIZE=$(ls -la --block-size=512 "$XV6_FS_IMG" | cut -d' ' -f 5 ) # If using fs.img


# Start sectors of XV6
KERNEL_START=34 # Start kernel partition after GPT data
FS_START=$(( $KERNEL_START + $KERNEL_SIZE ))


# Print out the sizes of the binaries in 512B blocks
echo -e "$NAME XV6 Kernel block size:      $KERNEL_SIZE"
echo -e "$NAME Filesystem image block size: $FS_IMG_SIZE" # If using fs.img

read -p $'\e[1;33mWarning:\e[0m Doing this will replace all data on this card. Continue? y/n: ' -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]] ; then
    DEVBASENAME=$(basename $SDCARD)
    CHECKMOUNT=$(lsblk | grep "$DEVBASENAME"2 | tr -s ' ' | cut -d' ' -f 7)
    
    if [ ! -z $CHECKMOUNT ] ; then
        sudo umount -v $CHECKMOUNT
    fi

    #Make empty image
    if [ ! -z $WIPECARD ] ; then
        echo -e "$NAME Wiping SD card. This could take a while."
        sudo dd if=/dev/zero of=$SDCARD bs=64k status=progress && sync
    fi

    # GUID Partition Tables (GPT)
    # ===============================================
    # -g Converts any existing mbr record to a gpt record
    # --clear clears any GPT partition table that already exists.
    # --set-alignment=1 that we want to align partition starting sectors
    # to 1 sector boundaries I think? This would normally be set to 2048
    # apparently.

    sudo sgdisk -z $SDCARD

    sleep 1
    
    echo -e "$NAME Creating GUID Partition Table"
    sudo sgdisk -g --clear --set-alignment=1 \
     --new=1:$KERNEL_START:+$KERNEL_SIZE: --change-name=1:'xv6kernel' \
     --new=2:$FS_START:-0 --change-name=2:'filesystem' \
     $SDCARD
     # If using raw fs.img, you might use +$FS_IMG_SIZE instead of -0 for partition 2
     # Adjust typecodes if necessary, although default Linux typecode might be fine.

    sudo partprobe $SDCARD

    sleep 3

    echo -e "$NAME Copying binaries into their partitions."
    DD_FLAGS="bs=4k iflag=direct,fullblock oflag=dsync conv=fsync status=progress"

    echo -e "$NAME Extracting and copying XV6 Kernel"
    # Extract raw binary and copy
    $OBJCOPY -O binary "$XV6_KERNEL" "$XV6_KERNEL_BIN" # Extract
    sudo dd if="$XV6_KERNEL_BIN" of="$SDCARD""$PART_PREFIX"1 $DD_FLAGS
    rm "$XV6_KERNEL_BIN" # Clean up the temporary file

    echo -e "$NAME Copying Filesystem Image"
    sudo dd if="$XV6_FS_IMG" of="$SDCARD""$PART_PREFIX"2 $DD_FLAGS


fi

echo
echo "GPT Information for $SDCARD ==================================="
sudo sgdisk -p $SDCARD
