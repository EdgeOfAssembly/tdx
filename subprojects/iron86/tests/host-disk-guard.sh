#!/usr/bin/env bash
# Host-disk guard for iron86/tdx FDISK work.
# Watches for host partition/format tools or emulator --hdc on a block device.
# On hit: SIGTERM the offender, print FAILED, exit 1.
# Silent otherwise. Do not print progress (monitor wakeups).
set -u
SELF=$$
LOG="${IRON86_DISK_GUARD_LOG:-/tmp/iron86-host-disk-guard.log}"
exec 3>>"$LOG"

is_host_block_path() {
    local p=$1
    p=${p#of=}
    p=${p#if=}
    [[ -z "$p" ]] && return 1
    [[ -b "$p" ]] && return 0
    case "$p" in
    /dev/sd* | /dev/hd* | /dev/vd* | /dev/nvme* | /dev/mapper/* | /dev/dm-* | /dev/loop*)
        return 0
        ;;
    esac
    return 1
}

fail_kill() {
    local pid=$1
    local why=$2
    printf '%s\n' "$why" >&3
    if [[ -n "$pid" && "$pid" != "$SELF" && "$pid" != "1" ]]; then
        kill -TERM "$pid" 2>/dev/null || true
        sleep 0.2
        kill -KILL "$pid" 2>/dev/null || true
    fi
    echo "FAILED: $why"
    exit 1
}

scan() {
    local d pid cmd comm w fd target
    for d in /proc/[0-9]*; do
        pid=${d#/proc/}
        [[ "$pid" == "$SELF" ]] && continue
        [[ -r "$d/cmdline" ]] || continue
        cmd=$(tr '\0' ' ' <"$d/cmdline" 2>/dev/null) || continue
        [[ -z "${cmd// /}" ]] && continue
        comm=$(cat "$d/comm" 2>/dev/null || echo "")

        case "$comm" in
        fdisk | sfdisk | cfdisk | parted | gdisk | sgdisk | wipefs | blkdiscard | partprobe | mkfs | mkfs.*)
            for w in $cmd; do
                if is_host_block_path "$w"; then
                    fail_kill "$pid" "host $comm pid=$pid argv=$cmd"
                fi
            done
            ;;
        esac
        case "$cmd" in
        *mkfs.fat* | *mkfs.vfat* | *mkfs.msdos* | *mkfs.ext* | *mke2fs*)
            for w in $cmd; do
                if is_host_block_path "$w"; then
                    fail_kill "$pid" "host mkfs pid=$pid argv=$cmd"
                fi
            done
            ;;
        esac

        case "$comm" in
        dd)
            for w in $cmd; do
                case "$w" in
                of=/dev/sd* | of=/dev/hd* | of=/dev/vd* | of=/dev/nvme* | of=/dev/mapper/* | of=/dev/dm-* | of=/dev/loop*)
                    fail_kill "$pid" "host dd pid=$pid argv=$cmd"
                    ;;
                esac
            done
            ;;
        esac

        case "$comm" in
        iron86 | tdx | tdxview)
            prev=
            for w in $cmd; do
                if [[ "$prev" == "--hdc" ]] && is_host_block_path "$w"; then
                    fail_kill "$pid" "emulator --hdc block pid=$pid argv=$cmd"
                fi
                case "$w" in
                --hdc=/dev/sd* | --hdc=/dev/nvme* | --hdc=/dev/hd* | --hdc=/dev/vd* | --hdc=/dev/mapper/*)
                    fail_kill "$pid" "emulator --hdc=block pid=$pid argv=$cmd"
                    ;;
                esac
                prev=$w
            done
            if [[ -d "$d/fd" ]]; then
                for fd in "$d/fd/"*; do
                    [[ -b "$fd" ]] || continue
                    target=$(readlink "$fd" 2>/dev/null || true)
                    fail_kill "$pid" "emulator block-fd pid=$pid fd=$target comm=$comm"
                done
            fi
            ;;
        esac
    done
}

while :; do
    scan
    sleep 1
done
