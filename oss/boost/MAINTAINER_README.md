### Notes for Future Maintainers

This was originally imported by @Austin-Lamb in December 2020.

The provenance information (where it came from and which commit) is stored in the file `cgmanifest.json` in the same directory as this readme.
Please update the provenance information in that file when ingesting an updated version of the dependent library.
That provenance file is automatically read and inventoried by Microsoft systems to ensure compliance with appropriate governance standards.

## What should be done to update this in the future?

1. Download the version of boost you want from boost.org.
2. Take the parts you want, but leave most of it behind since it's HUGE and will bloat the repo to take it all.  At the time of this writing, we only use small_vector.hpp and its dependencies as a header-only library.
3. Validate that the license in the root of the repository didn't change and update it if so. It is sitting in a version-specific subdirectory below this readme. 
   If it changed dramatically, ensure that it is still compatible with our license scheme. Also update the NOTICE file in the root of our repository to declare the third-party usage.
4. Submit the pull.
