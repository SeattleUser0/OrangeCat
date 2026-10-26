sizeof(dd_t) = 2672 bytes (state, all options; a slimmer LITE-only struct is smaller: see docs)

| config | instances | median us/block | p99 us/block | mean us/block | % of 2901 us (median) | ns/sample/inst (median) |
|---|---|---|---|---|---|---|
| DIGITAL 44k, 1 head | 1 | 8.48 | 15.23 | 8.63 | 0.29 | 66.2 |
| DIGITAL 44k, 1 head | 4 | 26.13 | 36.11 | 26.46 | 0.90 | 51.0 |
| LITE 44k, 2 heads (SKANK) | 1 | 10.93 | 19.19 | 11.25 | 0.38 | 85.4 |
| LITE 44k, 2 heads (SKANK) | 4 | 38.30 | 50.08 | 39.04 | 1.32 | 74.8 |
| LITE 44k, 4 heads (SPREAD) | 1 | 14.13 | 23.20 | 14.52 | 0.49 | 110.4 |
| LITE 44k, 4 heads (SPREAD) | 4 | 51.11 | 70.83 | 52.08 | 1.76 | 99.8 |
| LITE 22k HB, 2 heads | 1 | 11.78 | 19.60 | 11.93 | 0.41 | 92.0 |
| LITE 22k HB, 2 heads | 4 | 42.02 | 75.57 | 43.60 | 1.45 | 82.1 |
| LITE 22k HB, 4 heads | 1 | 13.31 | 20.90 | 13.42 | 0.46 | 104.0 |
| LITE 22k HB, 4 heads | 4 | 48.42 | 58.89 | 48.72 | 1.67 | 94.6 |
| LITE 22k box/lin, 2 heads | 1 | 8.41 | 15.52 | 8.58 | 0.29 | 65.7 |
| LITE 22k box/lin, 2 heads | 4 | 28.20 | 38.38 | 28.50 | 0.97 | 55.1 |
| LITE 11k HB, 2 heads | 1 | 12.28 | 20.41 | 12.38 | 0.42 | 95.9 |
| LITE 11k HB, 2 heads | 4 | 42.83 | 53.50 | 42.80 | 1.48 | 83.7 |
| TAPE 44k, 3 heads (TRIPLE) | 1 | 23.17 | 33.46 | 23.68 | 0.80 | 181.0 |
| TAPE 44k, 3 heads (TRIPLE) | 4 | 86.59 | 109.66 | 88.19 | 2.98 | 169.1 |
| TAPE 22k HB, 3 heads | 1 | 18.00 | 26.50 | 18.30 | 0.62 | 140.7 |
| TAPE 22k HB, 3 heads | 4 | 67.10 | 84.27 | 67.96 | 2.31 | 131.0 |
| TAPE 11k HB, 3 heads | 1 | 15.70 | 23.39 | 15.84 | 0.54 | 122.7 |
| TAPE 11k HB, 3 heads | 4 | 56.83 | 71.13 | 57.27 | 1.96 | 111.0 |
| TAPE 22k HB stereo (2 lines), 3 heads | 1 | 24.83 | 64.39 | 27.72 | 0.86 | 194.0 |
| TAPE 22k HB stereo (2 lines), 3 heads | 4 | 93.27 | 117.82 | 94.59 | 3.21 | 182.2 |
