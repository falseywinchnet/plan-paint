# Development conventions

First-party C++ is C++20 with explicit types, named callbacks and visible ownership. Do not use `auto`, lambdas, arrow member access or structured bindings. Numerical kernels use double precision without fast-math. Vendor code retains upstream style and license notices.

Preserve the classic Windows 7/10 Paint ribbon and the path, stamp, pattern and reshape tools. Test numerical and editing behavior, and inspect real rendered output. Validate each released platform independently.

Run CTest and `python3 scripts/check-style.py` before publishing changes. Keep development diaries, machine-specific receipts and working notes in the ignored `astra/` directory. Public prose must be self-contained. Credits use Author: Astra and Sponsor: Rainstar; the dedication belongs before the credits, never in a tagline.
