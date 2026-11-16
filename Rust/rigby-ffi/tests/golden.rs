//! Golden tests: replay reference frames from the JavaScript runtime through the C ABI.
//!
//! Committed frames live in `Tests/Golden`. Additional frames can be supplied with
//! `RIGAXIS_GOLDEN_DIR`; skeleton and atlas paths in a golden file are resolved relative to the
//! golden file first, then to the repository root.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::{Path, PathBuf};
use std::ptr;
use std::slice;

use rigaxis_ffi::*;
use rigby_runtime::json::Json;

const DT: f64 = 1.0 / 30.0;
const EVENT_COMPLETE: u32 = 2;
const EVENT_KEY: u32 = 3;

struct Skeleton(*mut RigAxisSkeleton);
impl Drop for Skeleton {
    fn drop(&mut self) {
        unsafe { rigaxis_skeleton_free(self.0) };
    }
}

struct Atlas(*mut RigAxisAtlas);
impl Drop for Atlas {
    fn drop(&mut self) {
        unsafe { rigaxis_atlas_free(self.0) };
    }
}

struct Lookup(*mut RigAxisLookup);
impl Drop for Lookup {
    fn drop(&mut self) {
        unsafe { rigaxis_lookup_free(self.0) };
    }
}

struct Pose(*mut RigAxisPose);
impl Drop for Pose {
    fn drop(&mut self) {
        unsafe { rigaxis_pose_free(self.0) };
    }
}

struct State(*mut RigAxisState);
impl Drop for State {
    fn drop(&mut self) {
        unsafe { rigaxis_state_free(self.0) };
    }
}

struct Render(*mut RigAxisRender);
impl Drop for Render {
    fn drop(&mut self) {
        unsafe { rigaxis_render_free(self.0) };
    }
}

struct Worst {
    world: f64,
    positions: f64,
    color: f64,
    at: String,
}

fn close(a: f64, b: f64, rel: f64) -> f64 {
    if a == b || (a.is_nan() && b.is_nan()) {
        return 0.0;
    }
    (a - b).abs() / 1f64.max(a.abs()).max(b.abs()) / rel
}

fn js_number(value: f64) -> String {
    if value == value.trunc() && value.abs() < 1e15 {
        format!("{}", value as i64)
    } else {
        format!("{value}")
    }
}

fn last_error() -> String {
    let text = rigaxis_last_error();
    if text.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(text) }
            .to_string_lossy()
            .into_owned()
    }
}

fn c_string(text: &str) -> CString {
    CString::new(text).expect("golden names contain no NUL")
}

fn c_text(text: *const c_char) -> String {
    if text.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(text) }
            .to_string_lossy()
            .into_owned()
    }
}

fn load_skeleton(path: &Path) -> Result<Skeleton, String> {
    let bytes = std::fs::read(path).map_err(|e| format!("cannot read {}: {e}", path.display()))?;
    let mut raw = ptr::null_mut();
    let status = unsafe { rigaxis_skeleton_create(bytes.as_ptr(), bytes.len(), &mut raw) };
    if status != RigAxisStatus::Ok {
        return Err(format!("cannot load {}: {}", path.display(), last_error()));
    }
    Ok(Skeleton(raw))
}

fn load_atlas(path: &Path) -> Result<Atlas, String> {
    let bytes = std::fs::read(path).map_err(|e| format!("cannot read {}: {e}", path.display()))?;
    let mut raw = ptr::null_mut();
    let status = unsafe { rigaxis_atlas_create(bytes.as_ptr(), bytes.len(), &mut raw) };
    if status != RigAxisStatus::Ok {
        return Err(format!("cannot load {}: {}", path.display(), last_error()));
    }
    Ok(Atlas(raw))
}

fn create_lookup(skeleton: &Skeleton, atlas: &Atlas) -> Result<Lookup, String> {
    let mut raw = ptr::null_mut();
    let status = unsafe { rigaxis_lookup_create(skeleton.0, atlas.0, &mut raw) };
    if status != RigAxisStatus::Ok {
        return Err(format!("cannot create region lookup: {}", last_error()));
    }
    Ok(Lookup(raw))
}

fn create_pose(skeleton: &Skeleton, skin: Option<&str>) -> Result<Pose, String> {
    let mut raw = ptr::null_mut();
    let status = unsafe { rigaxis_pose_create(skeleton.0, &mut raw) };
    if status != RigAxisStatus::Ok {
        return Err(format!("cannot create pose: {}", last_error()));
    }
    let pose = Pose(raw);
    if let Some(skin) = skin {
        let name = c_string(skin);
        if !unsafe { rigaxis_pose_set_skin(skeleton.0, pose.0, name.as_ptr()) } {
            return Err(format!("unknown skin '{skin}'"));
        }
    }
    Ok(pose)
}

fn create_state() -> Result<State, String> {
    let mut raw = ptr::null_mut();
    let status = unsafe { rigaxis_state_create(&mut raw) };
    if status != RigAxisStatus::Ok {
        return Err(format!("cannot create state: {}", last_error()));
    }
    Ok(State(raw))
}

fn create_render() -> Result<Render, String> {
    let mut raw = ptr::null_mut();
    let status = unsafe { rigaxis_render_create(&mut raw) };
    if status != RigAxisStatus::Ok {
        return Err(format!("cannot create render output: {}", last_error()));
    }
    Ok(Render(raw))
}

fn set_animation(
    skeleton: &Skeleton,
    state: &State,
    track: u32,
    name: &str,
    looping: bool,
    mix: f64,
) -> Result<(), String> {
    let name = c_string(name);
    let mut entry = 0u64;
    if !unsafe {
        rigaxis_state_set_animation(
            state.0,
            skeleton.0,
            track,
            name.as_ptr(),
            looping,
            mix,
            &mut entry,
        )
    } {
        return Err(format!("cannot play animation '{name:?}' on track {track}"));
    }
    Ok(())
}

fn setup_scenario(
    skeleton: &Skeleton,
    state: &State,
    kind: &str,
    names: &[&str],
    step: usize,
) -> Result<(), String> {
    match kind {
        "loop" | "skin" if step == 0 => set_animation(skeleton, state, 0, names[0], true, 0.0),
        "mix" => {
            if step == 0 {
                set_animation(skeleton, state, 0, names[0], true, 0.0)?;
            }
            if step == 12 {
                set_animation(skeleton, state, 0, names[1], true, 0.25)?;
                if unsafe {
                    rigaxis_state_set_animation(
                        state.0,
                        skeleton.0,
                        1,
                        c_string(names[2]).as_ptr(),
                        false,
                        0.0,
                        ptr::null_mut(),
                    )
                } {
                    unsafe { rigaxis_state_set_track_alpha(state.0, 1, 0.5) };
                }
                let mut entry = 0u64;
                if !unsafe {
                    rigaxis_state_add_animation(
                        state.0,
                        skeleton.0,
                        0,
                        c_string(names[0]).as_ptr(),
                        true,
                        0.2,
                        0.5,
                        true,
                        &mut entry,
                    )
                } {
                    return Err(format!("cannot queue animation '{}'", names[0]));
                }
            }
            Ok(())
        }
        _ => Ok(()),
    }
}

fn empty_affine() -> RigAxisAffine {
    RigAxisAffine {
        a: 0.0,
        b: 0.0,
        c: 0.0,
        d: 0.0,
        tx: 0.0,
        ty: 0.0,
    }
}

fn empty_batch() -> RigAxisBatch {
    RigAxisBatch {
        source: 0,
        page: 0,
        blend: 0,
        vertex_count: 0,
        index_count: 0,
        vertices: ptr::null(),
        indices: ptr::null(),
    }
}

fn empty_event() -> RigAxisEvent {
    RigAxisEvent {
        kind: 0,
        track: 0,
        animation: 0,
        entry: 0,
        animation_name: ptr::null(),
        event_name: ptr::null(),
        time: 0.0,
        loop_index: 0,
        int_value: 0,
        float_value: 0.0,
        string_value: ptr::null(),
        audio: ptr::null(),
        volume: 0.0,
        balance: 0.0,
    }
}

fn bone_world(pose: &Pose, bone: u32) -> Result<RigAxisAffine, String> {
    let mut affine = empty_affine();
    if !unsafe { rigaxis_pose_bone_world(pose.0, bone, &mut affine) } {
        return Err(format!("cannot read bone {bone}"));
    }
    Ok(affine)
}

fn render_vertices(render: &Render) -> Vec<RigAxisVertex> {
    let count = unsafe { rigaxis_render_batch_count(render.0) };
    let mut vertices = Vec::new();
    for index in 0..count {
        let mut batch = empty_batch();
        if !unsafe { rigaxis_render_batch_at(render.0, index, &mut batch) }
            || batch.vertex_count == 0
        {
            continue;
        }
        let slice = unsafe { slice::from_raw_parts(batch.vertices, batch.vertex_count as usize) };
        vertices.extend_from_slice(slice);
    }
    vertices
}

fn drain_events(state: &State) -> Vec<String> {
    let count = unsafe { rigaxis_state_event_count(state.0) };
    let mut events = Vec::new();
    for index in 0..count {
        let mut event = empty_event();
        if !unsafe { rigaxis_state_event_at(state.0, index, &mut event) } {
            continue;
        }
        match event.kind {
            EVENT_COMPLETE => events.push(format!(
                "{}:complete:{}",
                event.track,
                c_text(event.animation_name)
            )),
            EVENT_KEY => events.push(format!(
                "{}:{}@{}",
                event.track,
                c_text(event.event_name),
                js_number(event.time)
            )),
            _ => {}
        }
    }
    unsafe { rigaxis_state_clear_events(state.0) };
    events
}

fn replay(
    skeleton: &Skeleton,
    lookup: &Lookup,
    render: &Render,
    scenario: &Json,
    worst: &mut Worst,
    label: &str,
) -> Result<Vec<String>, String> {
    let kind = scenario.str_or("kind", "");
    let animation = scenario.str_or("animation", "");
    let skin = scenario.get("skin").as_str();
    let frames = scenario.get("frames").as_array();
    let names: Vec<&str> = animation.split(['>', '+']).collect();
    let (steps, every) = match kind {
        "setup" => (1, 1),
        "mix" => (45, 2),
        "skin" => (20, 5),
        _ => (frames.len() * 3, 3),
    };

    let pose = create_pose(skeleton, skin)?;
    let state = create_state()?;
    let mut problems = Vec::new();
    let mut events = Vec::new();
    let mut frame_index = 0;

    setup_scenario(skeleton, &state, kind, &names, 0)?;
    for step in 1..=steps {
        setup_scenario(skeleton, &state, kind, &names, step)?;
        unsafe {
            rigaxis_state_update(state.0, skeleton.0, DT);
            rigaxis_state_apply(state.0, skeleton.0, pose.0);
        }
        events.extend(drain_events(&state));

        if step % every != 0 {
            continue;
        }
        let Some(expected) = frames.get(frame_index) else {
            break;
        };
        frame_index += 1;
        let at = format!("{label} {kind} '{animation}' frame {frame_index}");

        let expected_world = expected.get("world").as_array();
        let bone_count = expected_world.len() / 6;
        for bone in 0..bone_count {
            let affine = bone_world(&pose, bone as u32)?;
            for (channel, value) in [affine.a, affine.b, affine.c, affine.d, affine.tx, affine.ty]
                .into_iter()
                .enumerate()
            {
                let expected_value = expected_world[bone * 6 + channel]
                    .as_f64()
                    .unwrap_or(f64::NAN);
                let error = close(value, expected_value, 1.0);
                if error > worst.world {
                    worst.world = error;
                    worst.at = format!(
                        "{at}, bone {bone} value {channel}: ffi {value} js {expected_value}"
                    );
                }
            }
        }

        let status = unsafe {
            rigaxis_render_build(
                render.0,
                skeleton.0,
                pose.0,
                lookup.0,
                [1.0f32, 1.0, 1.0, 1.0].as_ptr(),
            )
        };
        if status != RigAxisStatus::Ok {
            return Err(format!("{at}: render failed: {}", last_error()));
        }
        let vertices = render_vertices(render);
        let quads = expected.get("quads").as_array();
        let expected_vertices: usize = quads
            .iter()
            .map(|quad| quad.get("positions").as_array().len() / 2)
            .sum();
        if vertices.len() != expected_vertices {
            problems.push(format!(
                "{at}: {} vertices, expected {expected_vertices}",
                vertices.len()
            ));
        } else {
            let mut vertex = 0;
            for quad in quads {
                let positions = quad.get("positions").as_array();
                let color = quad.get("color").as_array();
                for corner in 0..positions.len() / 2 {
                    let actual = &vertices[vertex];
                    let x = positions[corner * 2].as_f64().unwrap_or(f64::NAN);
                    let y = positions[corner * 2 + 1].as_f64().unwrap_or(f64::NAN);
                    worst.positions = worst.positions.max(close(actual.x as f64, x, 1.0));
                    worst.positions = worst.positions.max(close(actual.y as f64, y, 1.0));
                    for (channel, value) in [
                        actual.light_r,
                        actual.light_g,
                        actual.light_b,
                        actual.light_a,
                    ]
                    .into_iter()
                    .enumerate()
                    {
                        let expected_color = color
                            .get(channel)
                            .and_then(Json::as_f64)
                            .unwrap_or(f64::NAN);
                        worst.color = worst.color.max(close(value as f64, expected_color, 1.0));
                    }
                    vertex += 1;
                }
            }
        }

        let expected_events: Vec<String> = expected
            .get("events")
            .as_array()
            .iter()
            .map(|value| value.as_str().unwrap_or("").to_string())
            .collect();
        if events != expected_events {
            problems.push(format!(
                "{at}: events {events:?}, expected {expected_events:?}"
            ));
        }
        events.clear();
    }

    if frame_index != frames.len() {
        problems.push(format!(
            "{label} {kind} '{animation}': {frame_index} of {} frames played",
            frames.len()
        ));
    }
    Ok(problems)
}

fn repo_root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../..")
        .canonicalize()
        .expect("repository root")
}

fn golden_files() -> Vec<PathBuf> {
    let mut directories = vec![repo_root().join("Tests/Golden")];
    if let Ok(extra) = std::env::var("RIGAXIS_GOLDEN_DIR") {
        directories.push(PathBuf::from(extra));
    }
    let mut files = Vec::new();
    for directory in directories {
        let entries = std::fs::read_dir(&directory)
            .unwrap_or_else(|_| panic!("cannot read {}", directory.display()));
        for entry in entries {
            let path = entry.expect("golden directory entry").path();
            if path.to_string_lossy().ends_with(".golden.json") {
                files.push(path);
            }
        }
    }
    files.sort();
    files
}

#[test]
fn matches_reference_frames() {
    let mut failures = Vec::new();
    let mut checked = 0;

    for file in golden_files() {
        let text = std::fs::read_to_string(&file).expect("read golden file");
        let golden = Json::parse(&text).expect("parse golden file");
        if golden.has("scene") {
            continue;
        }

        let label = golden.str_or("label", "?");
        let relative = golden.str_or("file", "");
        let golden_dir = file.parent().expect("golden directory");
        let skeleton_path = {
            let local = golden_dir.join(relative);
            if local.is_file() {
                local
            } else {
                repo_root().join(relative)
            }
        };
        let atlas_path = skeleton_path.with_extension("atlas");

        let skeleton = match load_skeleton(&skeleton_path) {
            Ok(skeleton) => skeleton,
            Err(error) => {
                failures.push(format!("{label}: {error}"));
                continue;
            }
        };
        let atlas = match load_atlas(&atlas_path) {
            Ok(atlas) => atlas,
            Err(error) => {
                failures.push(format!("{label}: {error}"));
                continue;
            }
        };
        let lookup = match create_lookup(&skeleton, &atlas) {
            Ok(lookup) => lookup,
            Err(error) => {
                failures.push(format!("{label}: {error}"));
                continue;
            }
        };
        let render = match create_render() {
            Ok(render) => render,
            Err(error) => {
                failures.push(format!("{label}: {error}"));
                continue;
            }
        };

        let bones = golden.get("bones").as_array().len();
        let actual_bones = unsafe { rigaxis_skeleton_bone_count(skeleton.0) } as usize;
        if bones != actual_bones {
            failures.push(format!("{label}: {actual_bones} bones, golden has {bones}"));
        }

        let mut worst = Worst {
            world: 0.0,
            positions: 0.0,
            color: 0.0,
            at: String::new(),
        };
        for scenario in golden.get("scenarios").as_array() {
            match replay(&skeleton, &lookup, &render, scenario, &mut worst, label) {
                Ok(problems) => failures.extend(problems),
                Err(error) => failures.push(format!("{label}: {error}")),
            }
        }
        println!(
            "{label}: max relative error world {:.2e}, positions {:.2e}, color {:.2e}",
            worst.world, worst.positions, worst.color
        );
        if worst.world > 1e-6 {
            failures.push(format!(
                "{label}: world error {:.2e} at {}",
                worst.world, worst.at
            ));
        }
        if worst.positions > 1e-6 {
            failures.push(format!("{label}: position error {:.2e}", worst.positions));
        }
        if worst.color > 1e-6 {
            failures.push(format!("{label}: color error {:.2e}", worst.color));
        }
        checked += 1;
    }

    assert!(checked > 0, "no golden files");
    assert!(failures.is_empty(), "\n{}", failures.join("\n"));
}
