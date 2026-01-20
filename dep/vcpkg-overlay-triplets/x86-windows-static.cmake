# Same as the official x86-windows-static triplet
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# ...but with explicit platform toolset, so that future toolsets
# aren't automatically picked up (it defaults to the latest one).
set(VCPKG_PLATFORM_TOOLSET v143)
