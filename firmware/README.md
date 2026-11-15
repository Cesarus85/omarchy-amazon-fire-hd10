# Private stock firmware input

`required.json` lists the four files and SHA256 hashes used on the reference
tablet. The actual blobs are not included.

Before removing Android, mount your own stock system/vendor filesystem read-only
and locate these exact filenames. Copy them into a private build-input directory
without changing their names. Preserve your own device calibration/identity
partitions; those are not interchangeable with firmware files.

`scripts/stage-platform.py` checks these hashes. A different firmware version
requires investigation and radio validation before changing the manifest.
Do not download random NVRAM or a “Wi-Fi fix” containing another tablet's identity.
Do not commit extracted firmware or calibration to this repository.
