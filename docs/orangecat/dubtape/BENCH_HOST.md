sizeof(dd_t) = 2832 bytes (state, all options; a slimmer LITE-only struct is smaller: see docs)

| config | instances | median us/block | p99 us/block | mean us/block | % of 2901 us (median) | ns/sample/inst (median) |
|---|---|---|---|---|---|---|
| DIGITAL 44k, 1 head | 1 | 5.99 | 21.83 | 6.62 | 0.21 | 46.8 |
| DIGITAL 44k, 1 head | 4 | 17.80 | 39.32 | 19.33 | 0.61 | 34.8 |
| LITE 44k, 2 heads (SKANK) | 1 | 7.00 | 20.76 | 7.57 | 0.24 | 54.6 |
| LITE 44k, 2 heads (SKANK) | 4 | 23.61 | 48.41 | 25.44 | 0.81 | 46.1 |
| LITE 44k, 4 heads (SPREAD) | 1 | 8.78 | 19.13 | 9.34 | 0.30 | 68.6 |
| LITE 44k, 4 heads (SPREAD) | 4 | 30.95 | 65.85 | 33.10 | 1.07 | 60.5 |
| LITE 22k HB, 2 heads | 1 | 7.06 | 15.66 | 7.44 | 0.24 | 55.2 |
| LITE 22k HB, 2 heads | 4 | 23.63 | 49.62 | 25.32 | 0.81 | 46.1 |
| LITE 22k HB, 4 heads | 1 | 7.92 | 23.91 | 8.47 | 0.27 | 61.9 |
| LITE 22k HB, 4 heads | 4 | 28.05 | 60.12 | 29.99 | 0.97 | 54.8 |
| LITE 22k box/lin, 2 heads | 1 | 5.58 | 18.63 | 6.04 | 0.19 | 43.6 |
| LITE 22k box/lin, 2 heads | 4 | 17.70 | 41.49 | 19.71 | 0.61 | 34.6 |
| LITE 11k HB, 2 heads | 1 | 7.36 | 20.45 | 8.10 | 0.25 | 57.5 |
| LITE 11k HB, 2 heads | 4 | 23.01 | 52.04 | 25.79 | 0.79 | 44.9 |
| TAPE 44k, 3 heads (TRIPLE) | 1 | 15.72 | 35.53 | 17.26 | 0.54 | 122.8 |
| TAPE 44k, 3 heads (TRIPLE) | 4 | 58.84 | 100.33 | 62.78 | 2.03 | 114.9 |
| TAPE 22k HB, 3 heads | 1 | 11.65 | 29.96 | 12.94 | 0.40 | 91.1 |
| TAPE 22k HB, 3 heads | 4 | 42.41 | 84.55 | 46.76 | 1.46 | 82.8 |
| TAPE 11k HB, 3 heads | 1 | 9.43 | 26.97 | 10.30 | 0.32 | 73.6 |
| TAPE 11k HB, 3 heads | 4 | 33.15 | 76.97 | 38.12 | 1.14 | 64.7 |
| TAPE 22k HB stereo (2 lines), 3 heads | 1 | 15.40 | 42.28 | 17.56 | 0.53 | 120.3 |
| TAPE 22k HB stereo (2 lines), 3 heads | 4 | 57.41 | 119.62 | 65.28 | 1.98 | 112.1 |
| (0.4) the spring alone, 22.05 kHz line | 1 | 6.95 | 22.77 | 7.59 | 0.24 | 54.3 |
| (0.4) the spring alone, 22.05 kHz line | 4 | 21.34 | 51.14 | 23.39 | 0.74 | 41.7 |
| (0.4) LITE 22k HB, 2 heads + the spring in the loop | 1 | 11.27 | 30.66 | 12.67 | 0.39 | 88.1 |
| (0.4) LITE 22k HB, 2 heads + the spring in the loop | 4 | 41.76 | 82.12 | 44.74 | 1.44 | 81.6 |
| (0.4) LITE 22k HB, 2 heads + the spring on the wet | 1 | 13.00 | 33.46 | 14.25 | 0.45 | 101.6 |
| (0.4) LITE 22k HB, 2 heads + the spring on the wet | 4 | 46.26 | 88.17 | 50.81 | 1.59 | 90.4 |
| (0.4) TAPE 22k HB, 3 heads + the spring in the loop | 1 | 16.12 | 36.18 | 17.93 | 0.56 | 125.9 |
| (0.4) TAPE 22k HB, 3 heads + the spring in the loop | 4 | 56.79 | 116.57 | 64.55 | 1.96 | 110.9 |
| (0.4) TAPE 22k HB, 3 heads + the spring on the wet | 1 | 17.72 | 41.12 | 19.08 | 0.61 | 138.5 |
| (0.4) TAPE 22k HB, 3 heads + the spring on the wet | 4 | 64.02 | 109.13 | 70.08 | 2.21 | 125.0 |
