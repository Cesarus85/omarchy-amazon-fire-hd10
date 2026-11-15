# Contributing

Please report the exact model/year, software versions, whether the system was
built from this repository, and a short reproducible sequence. Distinguish
“compiled”, “booted”, “visible” and “physically tested”; they are different evidence.

Do not upload full device-tree dumps, IDME/NVRAM, account data, keys, wireless
profiles, pairing databases, recordings or unfiltered diagnostics. Use
`scripts/inspect-device.sh` as a small read-only starting point and review
its output before sharing.

For code changes, run the source/profile/rotation tests and any relevant driver
regression tests. Kernel or suspend changes need hardware validation and a working
recovery route. Changes to exact dependencies need real update/reboot tests.

Keep imported authorship and license notices. Explain any new source provenance.
Use small focused pull requests, with validation and remaining limitations.
AI-assisted contributions are welcome here when identified and reviewed; respect
each upstream project's own contribution policy.
