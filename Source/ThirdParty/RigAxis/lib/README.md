# Native libraries

The Rust static libraries are built locally and are not stored in git.

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\BuildRust.ps1
```

The Win64 library is copied to `Win64/Release/rigaxis_ffi.lib` before building the Unreal Engine target.
