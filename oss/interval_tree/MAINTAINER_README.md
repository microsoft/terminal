# Notes for Future Maintainers

This was originally imported by @PankajBhojwani in September 2020.

The provenance information (where it came from and which commit) is stored in the file `cgmanifest.json` in the same directory as this readme.
Please update the provenance information in that file when ingesting an updated version of the dependent library.
That provenance file is automatically read and inventoried by Microsoft systems to ensure compliance with appropriate governance standards.

## Local deviations from upstream

**`IntervalTree.h` here is _not_ a byte-for-byte copy of upstream.** Do not assume it is.
There are two deliberate deviations, both required because we instantiate the tree with
`Scalar = til::point`, a user-defined struct rather than an arithmetic type:

1. **Struct keys.** `Scalar` is compared against `Scalar{}` instead of `0` (and `center` is
   default-initialized rather than set to `0`). This came in with the original import in
   #7691 and corresponds to the still-open upstream pull request
   <https://github.com/ekg/intervaltree/pull/31>. Without it this file does not compile
   against `til::point` at all.
2. **`is_valid()`.** Upstream seeds its bounds accumulators with
   `std::numeric_limits<Scalar>::max()`/`::min()`. `til::point` has no `std::numeric_limits`
   specialization, so both sentinels come back as a default-constructed `til::point{0,0}` and
   the subtree ordering checks fail for any tree large enough to have children — firing
   `assert(is_valid().first)` in Debug builds. Upstream additionally accumulates the *maximum*
   stop using `std::min`, which silently reduces those same checks to no-ops. Both are fixed
   here; see the comment in `is_valid()` and #20486.

Neither deviation has been accepted upstream, and upstream has had no commit since
2021-03-11, so they are expected to persist.

## What should be done to update this in the future?

1. Go to the ekg/intervaltree repository on GitHub.
2. Take the file IntervalTree.h wholesale and drop it into the directory here.
3. Don't change anything about it, **except** that you must re-apply the two local deviations
   listed above. Taking upstream wholesale without them will break the build, because
   `til::point` cannot be compared against `0`.
4. Validate that the license in the root of the repository didn't change and update it if so. It is sitting in the same directory as this readme.
   If it changed dramatically, ensure that it is still compatible with our license scheme. Also update the NOTICE file in the root of our repository to declare the third-party usage.
5. Submit the pull.

