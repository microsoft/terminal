# Notes for Future Maintainers

This was originally imported by @PankajBhojwani in September 2020.

The provenance information (where it came from and which commit) is stored in the file `cgmanifest.json` in the same directory as this readme.
Please update the provenance information in that file when ingesting an updated version of the dependent library.
That provenance file is automatically read and inventoried by Microsoft systems to ensure compliance with appropriate governance standards.

## Local deviation from upstream

**`IntervalTree.h` here is _not_ a byte-for-byte copy of upstream.** Do not assume it is.
`Scalar` is compared against `Scalar{}` instead of `0`, and `center` is default-initialized
rather than set to `0`, so that a user-defined struct such as `til::point` can be used as a
key. This came in with the original import in #7691 and corresponds to the still-open
upstream pull request <https://github.com/ekg/intervaltree/pull/31>. Without it this file
does not compile against `til::point` at all.

Note that `is_valid()` assumes `std::numeric_limits<Scalar>` is meaningful. We satisfy that
by specializing `std::numeric_limits` for the `til` coordinate types rather than by patching
this file; see `src/inc/til/point.h` and #20486.

## What should be done to update this in the future?

1. Go to the ekg/intervaltree repository on GitHub.
2. Take the file IntervalTree.h wholesale and drop it into the directory here.
3. Don't change anything about it, **except** that you must re-apply the local deviation
   listed above. Taking upstream wholesale without it will break the build, because
   `til::point` cannot be compared against `0`.
4. Validate that the license in the root of the repository didn't change and update it if so. It is sitting in the same directory as this readme.
   If it changed dramatically, ensure that it is still compatible with our license scheme. Also update the NOTICE file in the root of our repository to declare the third-party usage.
5. Submit the pull.

