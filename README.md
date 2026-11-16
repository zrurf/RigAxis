# RigAxis

A Rigby2D-compatible 2D skeletal animation runtime for Unreal Engine 5.

RigAxis plays Rigby2D skeleton and scene exports inside Unreal Engine, with a Blueprint-friendly component API and a native Rust core. It is an independent, unofficial project and is not affiliated with or endorsed by the Rigby2D project.

## Features

- Skeleton and atlas loading (`.skel`, skeleton `.json`, `.atlas`).
- Track playback with crossfades, queues, speed, and manual time control.
- Track timing and introspection: local time, current animation, duration, alpha, and time scale.
- Skeleton introspection: bone, animation, and skin names plus animation durations.
- Animation events exposed as Blueprint delegates.
- Skins and physics reset.
- Scene manual stepping, physics reset, and actor id/name enumeration.
- Custom `FPrimitiveSceneProxy` renderer with per-page, per-blend, and two-color tint materials.
- Default unlit sprite materials for normal, additive, multiply, and screen blending.
- Editor importers and reimport for skeleton and scene assets.
- Bone follower component for attaching actors or components to animated bones.
- Scene manifests (`.scene.json`) with stage parallax, actors, shots, and camera bones.

## Components

- `URigAxisComponent` — skeleton playback and rendering.
- `URigAxisBoneFollowerComponent` — follows a bone of a `URigAxisComponent`.
- `URigAxisSceneComponent` — plays a `URigAxisSceneAsset` and renders stage plus actors.

## Assets

- `URigAxisSkeletonAsset` — skeleton bytes, atlas bytes, atlas page textures, and load status.
- `URigAxisSceneAsset` — scene manifest plus the stage and actor skeleton assets.

## PaperZD adapter (optional)

`RigAxisPaperZD/` is a separate prototype plugin that drives a `URigAxisComponent` from PaperZD
playback through a custom animation source, sequence, and playback handle. It requires PaperZD 2.2
or newer and is not a dependency of the core plugin. See `RigAxisPaperZD/README.md` for setup and
current limitations.

## Building

Developed and tested with Unreal Engine 5.8, Visual Studio 2026, and Rust 1.99. Older engine versions are not verified yet.

### Windows

Build the native library (output is ignored by git):

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\BuildRust.ps1 -Target win64
```

Create the local test plugin shim once (ignored by git):

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\LinkTestPlugin.ps1
```

```powershell
& "<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" `
  RigAxisTestEditor Win64 Development `
  "<REPO>\Tests\RigAxisTest\RigAxisTest.uproject" `
  -WaitMutex
```

Run the Rust tests and clippy:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\TestRust.ps1
```

### Updating the bundled runtime

Check out the desired Rigby2D tag in an upstream checkout, then copy the runtime and update
`UPSTREAM.md` and `THIRD_PARTY_NOTICES.md`:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\SyncRuntime.ps1 `
  -Repo "<RIGBY2D_REPO>" -Ref v0.5.1
```

Then run `Scripts\TestRust.ps1`, rebuild the native library, and rerun the Unreal tests.

### Android

Build both Android architectures with the NDK:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\BuildRust.ps1 -Target android
```

`ANDROID_NDK_HOME` (or `NDK_HOME`) must point to the NDK. The Unreal Android platform component and SDK must also be installed for cook and packaging.

### macOS, iOS, Linux

Run the shell script on the matching host:

```bash
./Scripts/BuildRust.sh mac
./Scripts/BuildRust.sh ios
./Scripts/BuildRust.sh linux
./Scripts/BuildRust.sh all
```

The script copies libraries to the paths expected by `RigAxis.Build.cs`:

- `Source/ThirdParty/RigAxis/lib/Mac/rigaxis_ffi.a`
- `Source/ThirdParty/RigAxis/lib/iOS/arm64/rigaxis_ffi.a`
- `Source/ThirdParty/RigAxis/lib/iOS/simulator/rigaxis_ffi.a`
- `Source/ThirdParty/RigAxis/lib/Linux/x64/rigaxis_ffi.a`

## Sample content

- `Tests/RigAxisTest/Content/RigAxisTest/Map_RigAxisTest.umap` — skeleton component sample.
- `Tests/RigAxisTest/Content/RigAxisTest/Map_RigAxisScene.umap` — scene component sample.
- `Tests/Fixtures/` — original skeleton, atlas, and scene fixtures used by importers and tests.
- `Tests/Golden/` — frames recorded from the reference JavaScript runtime.

Run the automation suite:

```powershell
& "<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "<REPO>\Tests\RigAxisTest\RigAxisTest.uproject" `
  -ExecCmds="Automation RunTests RigAxis; Quit" `
  -unattended -nullrhi -nosplash -stdout `
  -TestExit="Automation Test Queue Empty"
```

Golden tests replay recorded skeleton and scene frames through the C ABI and compare bone
transforms, vertices, colors, draw lists, and events. To run additional local golden sets, point
`RIGAXIS_GOLDEN_DIR` at a directory of `.golden.json` files; skeleton and atlas paths resolve next
to the golden file first.

## Limitations

- Runtime only; there is no rigging or animation authoring editor.
- Scene playback does not emit per-animation events; use a `URigAxisComponent` for event-driven playback.
- Multiply and screen blending are approximations of the Rigby2D blend equations.
- Bone followers cannot represent shear; they use translation, rotation, and scale.
- Skeleton reimport refreshes skeleton and atlas data; changed atlas page PNGs are not reimported yet.
- Scene manifests are imported but not reimported.
- Only the Win64 build is currently exercised end to end in this repository.

## Trademarks

The Rigby2D, NILA and Rigbytron names, logos and artwork are trademarks or registered trademarks of the Rigby2D project. Other brands or product names are the trademarks of their respective owners.

## License

The plugin is released under the Apache License 2.0. The bundled Rigby2D runtime is available under MIT OR Apache-2.0; `THIRD_PARTY_NOTICES.md` contains the attribution.
