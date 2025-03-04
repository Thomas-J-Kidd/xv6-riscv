#!/bin/bash
###########################################
## flash-sd-xv6.sh
##
## Purpose: A script to flash an SD card with a bootable xv6 image.
## Based on the Linux flash-sd.sh script by Jacob Pease.
##
## A component of the CORE-V-WALLY configurable RISC-V project.
## https://github.com/openhwgroup/cvw
##
## Copyright (C) 2021-24 Harvey Mudd College & Oklahoma State University
##
## SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
################################################################################################

# Exit on any error
set -e

usage() { echo "Usage: $0 [-zh] [-x <path/to/xv6>] <device>" 1>&2; exit 1; }

help() {
    echo "Usage: $0 [OPTIONS] <device>"
    echo "  -z                     wipes card with zeros"
    echo "  -x <path/to/xv6>      path to xv6 directory"
    echo "  -d <device tree>      path to device tree file"
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

# Default values
XV6_PATH="."
DEVICE_TREE="wally-xv6.dtb"
MNT_DIR="xv6img"

# Process options and arguments
ARGS=()
while [ $OPTIND -le "$#" ] ; do
    if getopts "hzx:d:" arg ; then
        case "${arg}" in
            h) help
               ;;
            z) WIPECARD=y
               ;;
            x) XV6_PATH=${OPTARG}
               ;;
            d) DEVICE_TREE=${OPTARG}
               ;;
        esac
    else
        ARGS+=("${!OPTIND}")
        ((OPTIND++))
    fi
done

# File locations
XV6_KERNEL="$XV6_PATH/kernel/kernel"
FW_JUMP="$XV6_PATH/firmware/fw_jump.bin"

SDCARD=${ARGS[0]}

# User Error Checks ===================================================

if [ "$#" -eq "0" ] ; then
    usage
fi

# Check if SD card device exists
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

# Check if required files exist
if [ ! -e $XV6_KERNEL ] ; then
    echo -e "$ERRORTEXT xv6 kernel not found at $XV6_KERNEL"
    echo "       Build xv6 before running this script."
    exit 1
fi

if [ ! -e $FW_JUMP ] ; then
    echo -e "$ERRORTEXT OpenSBI firmware not found at $FW_JUMP"
    exit 1
fi

if [ ! -e $DEVICE_TREE ] ; then
    echo -e "$ERRORTEXT Device tree not found at $DEVICE_TREE"
    exit 1
fi

# Calculate partition information =====================================

# Fixed partition sizes in MB
DTB_SIZE=1      # 1MB for device tree
OPENSBI_SIZE=1  # 1MB for OpenSBI
KERNEL_SIZE=8   # 8MB for xv6 kernel

# Convert to 512B blocks
DTB_BLOCKS=$((DTB_SIZE * 2048))
OPENSBI_BLOCKS=$((OPENSBI_SIZE * 2048))
KERNEL_BLOCKS=$((KERNEL_SIZE * 2048))

# Calculate start sectors
OPENSBI_START=$((34 + DTB_BLOCKS))
KERNEL_START=$((OPENSBI_START + OPENSBI_BLOCKS))
FS_START=$((KERNEL_START + KERNEL_BLOCKS))

# Print partition information
echo -e "$NAME Partition layout (in 512B blocks):"
echo -e "$NAME Device tree:     $DTB_BLOCKS blocks starting at 34"
echo -e "$NAME OpenSBI:         $OPENSBI_BLOCKS blocks starting at $OPENSBI_START"
echo -e "$NAME xv6 kernel:      $KERNEL_BLOCKS blocks starting at $KERNEL_START"
echo -e "$NAME Filesystem:      Remaining space starting at $FS_START"

read -p $'\e[1;33mWarning:\e[0m Doing this will replace all data on this card. Continue? y/n: ' -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]] ; then
    DEVBASENAME=$(basename $SDCARD)
    CHECKMOUNT=$(lsblk | grep "$DEVBASENAME"4 | tr -s ' ' | cut -d' ' -f 7)
    
    if [ ! -z $CHECKMOUNT ] ; then
        sudo umount -v $CHECKMOUNT
    fi

    # Wipe card if requested
    if [ ! -z $WIPECARD ] ; then
        echo -e "$NAME Wiping SD card. This could take a while."
        sudo dd if=/dev/zero of=$SDCARD bs=64k status=progress && sync
    fi

    # Create GPT partition table
    sudo sgdisk -z $SDCARD
    sleep 1
    
    echo -e "$NAME Creating GUID Partition Table"
    sudo sgdisk -g --clear --set-alignment=1 \
         --new=1:34:+$DTB_BLOCKS --change-name=1:'fdt' \
         --new=2:$OPENSBI_START:+$OPENSBI_BLOCKS --change-name=2:'opensbi' --typecode=2:2E54B353-1271-4842-806F-E436D6AF6985 \
         --new=3:$KERNEL_START:+$KERNEL_BLOCKS --change-name=3:'xv6kernel' \
         --new=4:$FS_START:-0 --change-name=4:'xv6fs' \
         $SDCARD

    sudo partprobe $SDCARD
    sleep 3

    echo -e "$NAME Copying binaries into their partitions."
    DD_FLAGS="bs=4k iflag=fullblock oflag=direct conv=fsync status=progress"

    echo -e "$NAME Copying device tree"
    sudo dd if=$DEVICE_TREE of="$SDCARD""$PART_PREFIX"1 $DD_FLAGS

    echo -e "$NAME Copying OpenSBI"
    sudo dd if=$FW_JUMP of="$SDCARD""$PART_PREFIX"2 $DD_FLAGS

    echo -e "$NAME Copying xv6 kernel"
    sudo dd if=$XV6_KERNEL of="$SDCARD""$PART_PREFIX"3 $DD_FLAGS

    # Create xv6 filesystem
    echo -e "$NAME Creating xv6 filesystem"
    sudo mkfs.ext2 "$SDCARD""$PART_PREFIX"4
    sudo mkdir -p /mnt/$MNT_DIR
    sudo mount -v "$SDCARD""$PART_PREFIX"4 /mnt/$MNT_DIR

    # Here we would copy any initial files needed in the xv6 filesystem
    # For now, we just create an empty filesystem

    sudo umount -v /mnt/$MNT_DIR
    sudo rmdir /mnt/$MNT_DIR
fi

echo
echo "GPT Information for $SDCARD ==================================="
sudo sgdisk -p $SDCARD
