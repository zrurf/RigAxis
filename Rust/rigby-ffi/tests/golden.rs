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
use rigby_runtime::scene::read_scene_manifest;

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

struct Scene(*mut RigAxisScene);
impl Drop for Scene {
    fn drop(&mut self) {
        unsafe { rigaxis_scene_free(self.0) };
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

fn create_scene(manifest_path: &Path) -> Result<Scene, String> {
    let manifest_bytes = std::fs::read(manifest_path)
        .map_err(|e| format!("cannot read {}: {e}", manifest_path.display()))?;
    let manifest = read_scene_manifest(&manifest_bytes).map_err(|e| e.0)?;
    let directory = manifest_path.parent().unwrap_or_else(|| Path::new("."));

    let mut paths = vec![manifest.stage.clone()];
    for actor in &manifest.scene.actors {
        if let Some(skeleton) = &actor.skeleton {
            if !paths.iter().any(|path| path == skeleton) {
                paths.push(skeleton.clone());
            }
        }
    }

    let mut source_paths = Vec::new();
    let mut skeletons = Vec::new();
    let mut atlases = Vec::new();
    for path in paths {
        let skeleton_path = directory.join(&path);
        let atlas_path = skeleton_path.with_extension("atlas");
        let skeleton = std::fs::read(&skeleton_path)
            .map_err(|e| format!("cannot read {}: {e}", skeleton_path.display()))?;
        let atlas = std::fs::read(&atlas_path)
            .map_err(|e| format!("cannot read {}: {e}", atlas_path.display()))?;
        source_paths.push(c_string(&path));
        skeletons.push(skeleton);
        atlases.push(atlas);
    }

    let sources: Vec<RigAxisSceneSource> = (0..source_paths.len())
        .map(|index| RigAxisSceneSource {
            path: source_paths[index].as_ptr(),
            skeleton: skeletons[index].as_ptr(),
            skeleton_size: skeletons[index].len(),
            atlas: atlases[index].as_ptr(),
            atlas_size: atlases[index].len(),
        })
        .collect();

    let mut raw = ptr::null_mut();
    let status = unsafe {
        rigaxis_scene_create(
            manifest_bytes.as_ptr(),
            manifest_bytes.len(),
            sources.as_ptr(),
            sources.len() as u32,
            &mut raw,
        )
    };
    if status != RigAxisStatus::Ok {
        return Err(format!("cannot load scene: {}", last_error()));
    }
    Ok(Scene(raw))
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

fn compare_bone_world(
    affine: &RigAxisAffine,
    expected: &[Json],
    bone: usize,
    worst: &mut Worst,
    at: &str,
) {
    for (channel, value) in [affine.a, affine.b, affine.c, affine.d, affine.tx, affine.ty]
        .into_iter()
        .enumerate()
    {
        let expected_value = expected[bone * 6 + channel].as_f64().unwrap_or(f64::NAN);
        let error = close(value, expected_value, 1.0);
        if error > worst.world {
            worst.world = error;
            worst.at =
                format!("{at}, bone {bone} value {channel}: ffi {value} js {expected_value}");
        }
    }
}

fn render_vertices(render: &Render) -> Vec<RigAxisVertex> {
    render_vertices_filtered(render, None)
}

fn render_source_vertices(render: &Render, source: u32) -> Vec<RigAxisVertex> {
    render_vertices_filtered(render, Some(source))
}

fn render_vertices_filtered(render: &Render, source: Option<u32>) -> Vec<RigAxisVertex> {
    let count = unsafe { rigaxis_render_batch_count(render.0) };
    let mut vertices = Vec::new();
    for index in 0..count {
        let mut batch = empty_batch();
        if !unsafe { rigaxis_render_batch_at(render.0, index, &mut batch) }
            || batch.vertex_count == 0
            || source.is_some_and(|source| batch.source != source)
        {
            continue;
        }
        let slice = unsafe { slice::from_raw_parts(batch.vertices, batch.vertex_count as usize) };
        vertices.extend_from_slice(slice);
    }
    vertices
}

fn compare_quads(
    vertices: &[RigAxisVertex],
    quads: &[Json],
    compare_colors: bool,
    worst: &mut Worst,
    at: &str,
) -> Vec<String> {
    let mut problems = Vec::new();
    let expected_vertices: usize = quads
        .iter()
        .map(|quad| quad.get("positions").as_array().len() / 2)
        .sum();
    if vertices.len() != expected_vertices {
        problems.push(format!(
            "{at}: {} vertices, expected {expected_vertices}",
            vertices.len()
        ));
        return problems;
    }

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
            if compare_colors {
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
            }
            vertex += 1;
        }
    }
    problems
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
        problems.extend(compare_quads(&vertices, quads, true, worst, &at));

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

fn find_actor(scene: &Scene, id: &str) -> Option<u32> {
    let count = unsafe { rigaxis_scene_actor_count(scene.0) };
    for index in 0..count {
        let actor = unsafe { rigaxis_scene_actor_id(scene.0, index) };
        if c_text(actor) == id {
            return Some(index);
        }
    }
    None
}

fn find_source(scene: &Scene, id: &str) -> Option<u32> {
    let count = unsafe { rigaxis_scene_source_count(scene.0) };
    for index in 1..count {
        let source = unsafe { rigaxis_scene_source_id(scene.0, index) };
        if c_text(source) == id {
            return Some(index);
        }
    }
    None
}

fn replay_scene(
    scene: &Scene,
    render: &Render,
    golden: &Json,
    worst: &mut Worst,
    label: &str,
) -> Result<Vec<String>, String> {
    let mut problems = Vec::new();
    for shot in golden.get("shots").as_array() {
        let shot_name = shot.str_or("shot", "");
        let name = c_string(shot_name);
        unsafe { rigaxis_scene_play(scene.0, name.as_ptr(), true) };
        let frames = shot.get("frames").as_array();
        let mut frame_index = 0;

        for step in 1..=frames.len() * 3 {
            unsafe { rigaxis_scene_update(scene.0, DT) };
            if step % 3 != 0 {
                continue;
            }
            let Some(expected) = frames.get(frame_index) else {
                break;
            };
            frame_index += 1;
            let at = format!("{label} scene '{shot_name}' frame {frame_index}");

            let time = unsafe { rigaxis_scene_time(scene.0) };
            let expected_time = expected.num("time", f64::NAN);
            if close(time, expected_time, 1.0) > 1e-9 {
                problems.push(format!("{at}: time {time} expected {expected_time}"));
            }

            let stage = expected.get("stage").as_array();
            let bone_count = stage.len() / 6;
            let actual_bones = unsafe { rigaxis_scene_stage_bone_count(scene.0) } as usize;
            if actual_bones != bone_count {
                problems.push(format!(
                    "{at}: stage has {actual_bones} bones, expected {bone_count}"
                ));
            }
            for bone in 0..bone_count {
                let mut affine = empty_affine();
                if !unsafe { rigaxis_scene_stage_bone_world(scene.0, bone as u32, &mut affine) } {
                    return Err(format!("{at}: cannot read stage bone {bone}"));
                }
                compare_bone_world(&affine, stage, bone, worst, &at);
            }

            let layers = expected.get("layers").as_array();
            let actual_layers = unsafe { rigaxis_scene_layer_count(scene.0) } as usize;
            if actual_layers != layers.len() {
                problems.push(format!(
                    "{at}: {actual_layers} layers, expected {}",
                    layers.len()
                ));
            }
            for (index, layer) in layers.iter().enumerate() {
                let expected = layer.as_array();
                let mut slot = 0;
                let mut actor = -1;
                if !unsafe { rigaxis_scene_layer_at(scene.0, index as u32, &mut slot, &mut actor) }
                {
                    problems.push(format!("{at}: cannot read layer {index}"));
                    continue;
                }
                let expected_slot =
                    expected.first().and_then(Json::as_f64).unwrap_or(f64::NAN) as u32;
                let expected_actor =
                    expected.get(1).and_then(Json::as_f64).unwrap_or(f64::NAN) as i32;
                if slot != expected_slot || actor != expected_actor {
                    problems.push(format!(
                        "{at}: layer {index} is ({slot}, {actor}), expected ({expected_slot}, {expected_actor})"
                    ));
                }
            }

            let actors = expected.get("actors").as_array();
            if actors.iter().any(|actor| actor.get("quads").is_array()) {
                let status = unsafe { rigaxis_scene_build_render(scene.0, render.0) };
                if status != RigAxisStatus::Ok {
                    return Err(format!("{at}: scene build failed: {}", last_error()));
                }
            }
            for actor in actors {
                let id = actor.str_or("id", "");
                let Some(actor_index) = find_actor(scene, id) else {
                    problems.push(format!("{at}: no actor '{id}'"));
                    continue;
                };
                let world = actor.get("world").as_array();
                let bone_count = world.len() / 6;
                let actual_bones =
                    unsafe { rigaxis_scene_actor_bone_count(scene.0, actor_index) } as usize;
                if actual_bones != bone_count {
                    problems.push(format!(
                        "{at}: actor '{id}' has {actual_bones} bones, expected {bone_count}"
                    ));
                }
                for bone in 0..bone_count {
                    let mut affine = empty_affine();
                    if !unsafe {
                        rigaxis_scene_actor_bone_world_at(
                            scene.0,
                            actor_index,
                            bone as u32,
                            &mut affine,
                        )
                    } {
                        return Err(format!("{at}: cannot read actor '{id}' bone {bone}"));
                    }
                    compare_bone_world(&affine, world, bone, worst, &at);
                }
                let quads = actor.get("quads");
                if quads.is_array() {
                    let Some(source) = find_source(scene, id) else {
                        problems.push(format!("{at}: no render source for actor '{id}'"));
                        continue;
                    };
                    let vertices = render_source_vertices(render, source);
                    problems.extend(compare_quads(
                        &vertices,
                        quads.as_array(),
                        false,
                        worst,
                        &at,
                    ));
                }
            }
        }

        if frame_index != frames.len() {
            problems.push(format!(
                "{label} scene '{shot_name}': {frame_index} of {} frames played",
                frames.len()
            ));
        }
    }
    Ok(problems)
}

#[test]
fn scenes_match_reference_frames() {
    let mut failures = Vec::new();
    let mut checked = 0;

    for file in golden_files() {
        let text = std::fs::read_to_string(&file).expect("read golden file");
        let golden = Json::parse(&text).expect("parse golden file");
        if !golden.has("scene") {
            continue;
        }

        let label = golden.str_or("label", "?");
        let relative = golden.str_or("scene", "");
        let golden_dir = file.parent().expect("golden directory");
        let scene_path = {
            let local = golden_dir.join(relative);
            if local.is_file() {
                local
            } else {
                repo_root().join(relative)
            }
        };

        let scene = match create_scene(&scene_path) {
            Ok(scene) => scene,
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

        let mut worst = Worst {
            world: 0.0,
            positions: 0.0,
            color: 0.0,
            at: String::new(),
        };
        match replay_scene(&scene, &render, &golden, &mut worst, label) {
            Ok(problems) => failures.extend(problems),
            Err(error) => failures.push(format!("{label}: {error}")),
        }
        println!(
            "{label} scene: max relative error world {:.2e}, positions {:.2e}",
            worst.world, worst.positions
        );
        if worst.world > 1e-6 {
            failures.push(format!(
                "{label} scene: world error {:.2e} at {}",
                worst.world, worst.at
            ));
        }
        if worst.positions > 1e-6 {
            failures.push(format!(
                "{label} scene: position error {:.2e}",
                worst.positions
            ));
        }
        checked += 1;
    }

    assert!(checked > 0, "no scene golden files");
    assert!(failures.is_empty(), "\n{}", failures.join("\n"));
}
