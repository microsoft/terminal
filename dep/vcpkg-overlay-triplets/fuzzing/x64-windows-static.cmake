# Same as the official x64-windows-static triplet
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# ...but with explicit platform toolset, so that future toolsets
# aren't automatically picked up (it defaults to the latest one).
set(VCPKG_PLATFORM_TOOLSET v143)

set(VCPKG_CXX_FLAGS /fsanitize=address)
set(VCPKG_C_FLAGS /fsanitize=address)
