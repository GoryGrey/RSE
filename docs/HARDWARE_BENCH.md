# Hardware Benchmarking (Apples-to-Apples)

Use this guide to generate real hardware results for RSE and Linux on the same machine.
The goal is a clean, reproducible comparison with captured logs.

## Prereqs

- A USB stick (>= 1GB).
- The target hardware should allow USB boot (UEFI).
- Optional: another machine with a USB-serial adapter if you want serial logs.

## Build the ISO

```bash
cd /home/gorygrey/Apps/RSE
make -f boot/Makefile.uefi RSE_SINGLE_TORUS=1 RSE_BENCH_SMOKE=0
```

The ISO will be at `build/boot/rse_efi.iso`.

## Write the ISO to USB

```bash
sudo dd if=build/boot/rse_efi.iso of=/dev/sdX bs=4M status=progress oflag=sync
```

Replace `/dev/sdX` with your USB device.

## Run the RSE benchmarks on hardware

1) Boot the target machine from the USB.
2) Wait for the run to finish (look for `[RSE] benchmarks end` on screen).
3) Capture the results:
   - If you have serial logging, save the output to a file.
   - Otherwise, take clear photos of the benchmark output.

Save the log or photos with a unique name, for example:
- `benchmarks/uefi_single_torus_hw.log`
- `benchmarks/uefi_single_torus_hw_YYYYMMDD.jpg`

## Run the Linux baseline on the same hardware

Boot the same machine into Linux and run:

```bash
cd /home/gorygrey/Apps/RSE
sudo ./scripts/run_linux_baseline.sh
```

This updates `benchmarks/linux_baseline.json`.

## What to hand off

- `benchmarks/uefi_single_torus_hw.log` (or photos)
- `benchmarks/linux_baseline.json`
- Hardware details (CPU model, RAM, storage, and BIOS/UEFI version if possible)
