sizeof(dd_t) = 2856 bytes (state, all options; a slimmer LITE-only struct is smaller: see docs)

| config | instances | median us/block | p99 us/block | mean us/block | % of 2901 us (median) | ns/sample/inst (median) |
|---|---|---|---|---|---|---|
| DIGITAL 44k, 1 head | 1 | 6.30 | 15.24 | 6.61 | 0.22 | 49.2 |
| DIGITAL 44k, 1 head | 4 | 18.61 | 44.07 | 20.10 | 0.64 | 36.4 |
| LITE 44k, 2 heads (SKANK) | 1 | 7.47 | 22.30 | 8.08 | 0.26 | 58.4 |
| LITE 44k, 2 heads (SKANK) | 4 | 27.46 | 61.15 | 30.17 | 0.95 | 53.6 |
| LITE 44k, 4 heads (SPREAD) | 1 | 10.02 | 30.30 | 11.89 | 0.35 | 78.3 |
| LITE 44k, 4 heads (SPREAD) | 4 | 33.37 | 70.83 | 36.36 | 1.15 | 65.2 |
| LITE 22k HB, 2 heads | 1 | 7.30 | 23.33 | 8.15 | 0.25 | 57.1 |
| LITE 22k HB, 2 heads | 4 | 25.64 | 58.88 | 28.14 | 0.88 | 50.1 |
| LITE 22k HB, 4 heads | 1 | 8.71 | 27.25 | 9.91 | 0.30 | 68.1 |
| LITE 22k HB, 4 heads | 4 | 28.71 | 64.37 | 31.34 | 0.99 | 56.1 |
| LITE 22k box/lin, 2 heads | 1 | 5.92 | 22.06 | 6.72 | 0.20 | 46.2 |
| LITE 22k box/lin, 2 heads | 4 | 18.31 | 47.01 | 20.99 | 0.63 | 35.8 |
| LITE 11k HB, 2 heads | 1 | 7.46 | 25.81 | 8.34 | 0.26 | 58.3 |
| LITE 11k HB, 2 heads | 4 | 23.87 | 56.84 | 26.87 | 0.82 | 46.6 |
| TAPE 44k, 3 heads (TRIPLE) | 1 | 16.17 | 40.45 | 18.66 | 0.56 | 126.3 |
| TAPE 44k, 3 heads (TRIPLE) | 4 | 61.95 | 125.83 | 69.07 | 2.14 | 121.0 |
| TAPE 22k HB, 3 heads | 1 | 11.75 | 30.44 | 12.66 | 0.41 | 91.8 |
| TAPE 22k HB, 3 heads | 4 | 43.08 | 88.61 | 47.13 | 1.48 | 84.1 |
| TAPE 11k HB, 3 heads | 1 | 9.47 | 28.00 | 10.51 | 0.33 | 74.0 |
| TAPE 11k HB, 3 heads | 4 | 33.78 | 76.01 | 38.28 | 1.16 | 66.0 |
| TAPE 22k HB stereo (2 lines), 3 heads | 1 | 16.25 | 37.79 | 17.55 | 0.56 | 127.0 |
| TAPE 22k HB stereo (2 lines), 3 heads | 4 | 60.14 | 118.25 | 64.47 | 2.07 | 117.5 |
| (0.4) the spring alone, 22.05 kHz line | 1 | 7.08 | 23.35 | 7.66 | 0.24 | 55.3 |
| (0.4) the spring alone, 22.05 kHz line | 4 | 21.47 | 49.62 | 23.48 | 0.74 | 41.9 |
| (0.4) LITE 22k HB, 2 heads + the spring in the loop | 1 | 9.88 | 28.17 | 10.67 | 0.34 | 77.2 |
| (0.4) LITE 22k HB, 2 heads + the spring in the loop | 4 | 34.05 | 74.92 | 38.00 | 1.17 | 66.5 |
| (0.4) LITE 22k HB, 2 heads + the spring on the wet | 1 | 13.25 | 33.74 | 14.64 | 0.46 | 103.5 |
| (0.4) LITE 22k HB, 2 heads + the spring on the wet | 4 | 46.51 | 89.98 | 49.96 | 1.60 | 90.8 |
| (0.4) TAPE 22k HB, 3 heads + the spring in the loop | 1 | 13.60 | 34.36 | 15.01 | 0.47 | 106.2 |
| (0.4) TAPE 22k HB, 3 heads + the spring in the loop | 4 | 52.87 | 112.35 | 58.68 | 1.82 | 103.3 |
| (0.4) TAPE 22k HB, 3 heads + the spring on the wet | 1 | 17.52 | 42.01 | 19.01 | 0.60 | 136.9 |
| (0.4) TAPE 22k HB, 3 heads + the spring on the wet | 4 | 66.31 | 124.21 | 72.38 | 2.29 | 129.5 |
| (0.5) LITE 22k HB, 2 heads + the limiter | 1 | 7.41 | 23.74 | 8.27 | 0.26 | 57.9 |
| (0.5) LITE 22k HB, 2 heads + the limiter | 4 | 25.53 | 61.03 | 29.00 | 0.88 | 49.9 |
| (0.5) LITE 22k HB, 2 heads, the input cut held | 1 | 25.16 | 54.67 | 26.18 | 0.87 | 196.5 |
| (0.5) LITE 22k HB, 2 heads, the input cut held | 4 | 97.26 | 160.65 | 99.51 | 3.35 | 190.0 |
| (0.5) LITE 22k HB, 2 heads, the cut toggled (ramps) | 1 | 7.29 | 25.91 | 8.39 | 0.25 | 57.0 |
| (0.5) LITE 22k HB, 2 heads, the cut toggled (ramps) | 4 | 25.09 | 60.19 | 28.28 | 0.86 | 49.0 |
| (0.5) LITE 22k HB, 2 heads + the limiter + the cut toggled | 1 | 7.45 | 25.93 | 8.85 | 0.26 | 58.2 |
| (0.5) LITE 22k HB, 2 heads + the limiter + the cut toggled | 4 | 25.38 | 63.78 | 28.77 | 0.87 | 49.6 |
| (0.5) TAPE 22k HB, 3 heads + the limiter + the cut toggled | 1 | 12.07 | 30.98 | 13.17 | 0.42 | 94.3 |
| (0.5) TAPE 22k HB, 3 heads + the limiter + the cut toggled | 4 | 44.33 | 97.47 | 50.21 | 1.53 | 86.6 |
| (0.5) LITE 22k HB, 2 heads + the spring in the loop + the limiter + the cut toggled | 1 | 9.95 | 29.77 | 11.60 | 0.34 | 77.7 |
| (0.5) LITE 22k HB, 2 heads + the spring in the loop + the limiter + the cut toggled | 4 | 35.23 | 80.97 | 40.02 | 1.21 | 68.8 |
