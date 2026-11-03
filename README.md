# RigAxis

A Rigby2D-compatible 2D skeletal animation runtime for Unreal Engine 5.

RigAxis plays Rigby2D skeleton and scene exports inside Unreal Engine, with a Blueprint-friendly component API and a native Rust core. It is an independent, unofficial project and is not affiliated with or endorsed by the Rigby2D project.

## Status

Early development. The current milestone is the repository skeleton: plugin modules, a host project, and the `RigAxis.Version` console command.

## Repository layout

- `Source/RigAxis` — runtime module.
- `Source/RigAxisEditor` — editor-only module; importers will live here.
- `Tests/RigAxisTest` — host project used for local builds and automation.
- `dev-note/` — local design notes, not versioned.

## Building

Requires Unreal Engine 5.8, Visual Studio 2026, and Rust 1.99 on Windows.

Build the native library (the output is ignored by git):

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\BuildRust.ps1
```

Create the local test plugin shim once (ignored by git):

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\LinkTestPlugin.ps1
```

```powershell
& "E:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  RigAxisTestEditor Win64 Development `
  -Project="E:\Projects\URigby2D\Tests\RigAxisTest\RigAxisTest.uproject" `
  -WaitMutex
```

Run the version command:

```powershell
& "E:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "E:\Projects\URigby2D\Tests\RigAxisTest\RigAxisTest.uproject" `
  -ExecCmds="RigAxis.Version,Quit" -unattended -nullrhi -nosplash -stdout
```

## Trademarks

The Rigby2D, NILA and Rigbytron names, logos and artwork are trademarks or registered trademarks of the Rigby2D project. Other brands or product names are the trademarks of their respective owners.

## License

The plugin is planned to be released under the Apache License 2.0. The bundled Rigby2D runtime is available under MIT OR Apache-2.0; a `THIRD_PARTY_NOTICES.md` is included with attribution.
