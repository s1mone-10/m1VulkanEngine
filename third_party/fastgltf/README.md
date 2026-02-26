# fastgltf note

This folder is no longer used as the default installation path.

`fastgltf` is now fetched automatically by CMake (`FetchContent`) during the
configure step, together with `simdjson`.

You can still vendor the library manually if desired, but it is not required
for a normal build.
