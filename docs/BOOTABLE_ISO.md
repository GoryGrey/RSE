# Bootable ISO Workflow

Use these steps to build and run a bootable ISO for real workload testing.

## Build ISO

```
./scripts/build_iso.sh
```

Output:
- `build/boot/rse_efi.iso`

## Run ISO in QEMU

```
./scripts/run_iso.sh
```

## Scripted workloads

The ring3 init process will execute `/persist/boot.rc` or `/boot.rc` if present.
Use `boot/boot.rc.sample` as a starting point for real workloads.
