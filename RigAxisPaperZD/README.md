# RigAxis PaperZD Adapter

Optional runtime adapter that lets PaperZD animation playback drive a `URigAxisComponent`.

## Status

Prototype. It validates PaperZD's animation source and playback-handle extension points.

## What it does

- `URigAxisPaperZDAnimationSource` uses `URigAxisComponent` as the render component.
- `URigAxisPaperZDAnimSequence` names a RigAxis animation and reports its duration.
- `URigAxisPaperZDPlaybackHandle` selects the highest-weight sequence, starts the matching RigAxis animation, and sets track 0 to the PaperZD playback time.

## Requirements

- RigAxis
- PaperZD 2.2 or newer (tested with PaperZD 2.2.4 on Unreal Engine 5.8)

## Limitations

- No editor asset factory yet; create the animation source and sequences in C++ (or add a factory later).
- Weighted blending, animation layers, skins, and PaperZD notifies are not mapped.
- The adapter drives one RigAxis track; it does not replace PaperZD rendering for flipbooks.
