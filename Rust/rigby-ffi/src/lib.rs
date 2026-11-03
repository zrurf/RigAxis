//! C ABI for the Rigby2D runtime used by the RigAxis Unreal Engine plugin.
//!
//! Handles are opaque. Strings are UTF-8, null-terminated, and stay valid until the owning
//! handle is mutated or freed. Callers must use a single thread per handle. Functions that take
//! a handle require a live handle from the matching `*_create` call; freeing it invalidates it.

#![allow(clippy::missing_safety_doc)]

use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::slice;

use rigby_runtime::model::Blend;
use rigby_runtime::scene::{read_scene_manifest, ScenePlayer};
use rigby_runtime::{
    load_scene, load_skeleton, AnimationState, Atlas, Pose, RegionLookup, RenderOutput,
    SkeletonData, StateEventKind,
};

pub const RIGAXIS_ABI_VERSION: u32 = 1;
const VERSION: &[u8] = b"0.1.0\0";

#[repr(i32)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum RigAxisStatus {
    Ok = 0,
    InvalidArgument = 1,
    InvalidData = 2,
    OutOfMemory = 3,
    Unsupported = 4,
    Internal = 5,
}

thread_local! {
    static LAST_ERROR: RefCell<CString> = RefCell::new(CString::new("").unwrap());
}

fn clear_error() {
    LAST_ERROR.with(|slot| *slot.borrow_mut() = CString::new("").unwrap());
}

fn set_error(message: impl AsRef<str>) {
    let text = message.as_ref().replace('\0', " ");
    LAST_ERROR.with(|slot| *slot.borrow_mut() = CString::new(text).unwrap());
}

fn c_string(text: &str) -> CString {
    CString::new(text.replace('\0', " ")).unwrap()
}

fn guard(f: impl FnOnce() -> Result<(), String>) -> RigAxisStatus {
    match catch_unwind(AssertUnwindSafe(f)) {
        Ok(Ok(())) => RigAxisStatus::Ok,
        Ok(Err(message)) => {
            set_error(message);
            RigAxisStatus::InvalidData
        }
        Err(_) => {
            set_error("panic in the Rigby2D runtime");
            RigAxisStatus::Internal
        }
    }
}

pub struct RigAxisSkeleton {
    data: SkeletonData,
    format: CString,
    bone_names: Vec<CString>,
    animation_names: Vec<CString>,
    skin_names: Vec<CString>,
    warnings: Vec<CString>,
}

pub struct RigAxisAtlas {
    atlas: Atlas,
    page_names: Vec<CString>,
}

pub struct RigAxisLookup {
    lookup: RegionLookup,
    missing: Vec<CString>,
}

#[repr(C)]
pub struct RigAxisAffine {
    pub a: f64,
    pub b: f64,
    pub c: f64,
    pub d: f64,
    pub tx: f64,
    pub ty: f64,
}

pub struct RigAxisPose {
    pose: Pose,
}

pub struct RigAxisState {
    state: AnimationState,
    events: Vec<EventView>,
}

struct EventView {
    kind: u32,
    track: u32,
    animation: u32,
    entry: u64,
    animation_name: CString,
    event_name: Option<CString>,
    time: f64,
    loop_index: i64,
    int_value: i64,
    float_value: f64,
    string_value: Option<CString>,
    audio: Option<CString>,
    volume: f64,
    balance: f64,
}

#[repr(C)]
pub struct RigAxisEvent {
    pub kind: u32,
    pub track: u32,
    pub animation: u32,
    pub entry: u64,
    pub animation_name: *const c_char,
    pub event_name: *const c_char,
    pub time: f64,
    pub loop_index: i64,
    pub int_value: i64,
    pub float_value: f64,
    pub string_value: *const c_char,
    pub audio: *const c_char,
    pub volume: f64,
    pub balance: f64,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct RigAxisVertex {
    pub x: f32,
    pub y: f32,
    pub u: f32,
    pub v: f32,
    pub light_r: f32,
    pub light_g: f32,
    pub light_b: f32,
    pub light_a: f32,
    pub dark_r: f32,
    pub dark_g: f32,
    pub dark_b: f32,
    pub dark_a: f32,
}

#[repr(C)]
pub struct RigAxisBatch {
    pub source: u32,
    pub page: u32,
    pub blend: u32,
    pub vertex_count: u32,
    pub index_count: u32,
    pub vertices: *const RigAxisVertex,
    pub indices: *const u32,
}

pub struct RigAxisRender {
    output: RenderOutput,
    batches: Vec<RenderBatchView>,
}

#[derive(Default)]
struct RenderBatchView {
    source: u32,
    page: u32,
    blend: u32,
    vertices: Vec<RigAxisVertex>,
    indices: Vec<u32>,
}

pub struct RigAxisScene {
    player: ScenePlayer,
    stage_lookup: RegionLookup,
    character_lookups: Vec<RegionLookup>,
    source_ids: Vec<CString>,
    actor_ids: Vec<CString>,
    actor_names: Vec<CString>,
    current_shot: CString,
}

#[repr(C)]
pub struct RigAxisSceneSource {
    pub path: *const c_char,
    pub skeleton: *const u8,
    pub skeleton_size: usize,
    pub atlas: *const u8,
    pub atlas_size: usize,
}

fn skeleton_ref<'a>(skeleton: *const RigAxisSkeleton) -> Option<&'a RigAxisSkeleton> {
    if skeleton.is_null() {
        None
    } else {
        Some(unsafe { &*skeleton })
    }
}

fn atlas_ref<'a>(atlas: *const RigAxisAtlas) -> Option<&'a RigAxisAtlas> {
    if atlas.is_null() {
        None
    } else {
        Some(unsafe { &*atlas })
    }
}

fn lookup_ref<'a>(lookup: *const RigAxisLookup) -> Option<&'a RigAxisLookup> {
    if lookup.is_null() {
        None
    } else {
        Some(unsafe { &*lookup })
    }
}

fn scene_ref<'a>(scene: *const RigAxisScene) -> Option<&'a RigAxisScene> {
    if scene.is_null() {
        None
    } else {
        Some(unsafe { &*scene })
    }
}

fn scene_mut<'a>(scene: *mut RigAxisScene) -> Option<&'a mut RigAxisScene> {
    if scene.is_null() {
        None
    } else {
        Some(unsafe { &mut *scene })
    }
}

fn affine_value(affine: &rigby_runtime::math::Affine) -> RigAxisAffine {
    RigAxisAffine {
        a: affine.a,
        b: affine.b,
        c: affine.c,
        d: affine.d,
        tx: affine.tx,
        ty: affine.ty,
    }
}

fn pose_ref<'a>(pose: *const RigAxisPose) -> Option<&'a RigAxisPose> {
    if pose.is_null() {
        None
    } else {
        Some(unsafe { &*pose })
    }
}

fn state_ref<'a>(state: *const RigAxisState) -> Option<&'a RigAxisState> {
    if state.is_null() {
        None
    } else {
        Some(unsafe { &*state })
    }
}

fn collect_events(state: &mut RigAxisState, skeleton: &RigAxisSkeleton) {
    for event in state.state.drain_events() {
        let animation_name = skeleton
            .data
            .animations
            .get(event.animation)
            .map(|animation| c_string(&animation.name))
            .unwrap_or_else(|| c_string(""));
        let track = event.track as u32;
        let animation = event.animation as u32;
        let entry = event.entry;

        let view = match event.kind {
            StateEventKind::Start => EventView {
                kind: 0,
                track,
                animation,
                entry,
                animation_name,
                event_name: None,
                time: 0.0,
                loop_index: 0,
                int_value: 0,
                float_value: 0.0,
                string_value: None,
                audio: None,
                volume: 1.0,
                balance: 0.0,
            },
            StateEventKind::End => EventView {
                kind: 1,
                track,
                animation,
                entry,
                animation_name,
                event_name: None,
                time: 0.0,
                loop_index: 0,
                int_value: 0,
                float_value: 0.0,
                string_value: None,
                audio: None,
                volume: 1.0,
                balance: 0.0,
            },
            StateEventKind::Complete => EventView {
                kind: 2,
                track,
                animation,
                entry,
                animation_name,
                event_name: None,
                time: 0.0,
                loop_index: 0,
                int_value: 0,
                float_value: 0.0,
                string_value: None,
                audio: None,
                volume: 1.0,
                balance: 0.0,
            },
            StateEventKind::Event(key) => EventView {
                kind: 3,
                track,
                animation,
                entry,
                animation_name,
                event_name: Some(c_string(&key.name)),
                time: key.time,
                loop_index: key.loop_index,
                int_value: key.int,
                float_value: key.float,
                string_value: key.string.as_deref().map(c_string),
                audio: key.audio.as_deref().map(c_string),
                volume: key.volume,
                balance: key.balance,
            },
        };
        state.events.push(view);
    }
}

fn render_ref<'a>(render: *const RigAxisRender) -> Option<&'a RigAxisRender> {
    if render.is_null() {
        None
    } else {
        Some(unsafe { &*render })
    }
}

fn render_mut<'a>(render: *mut RigAxisRender) -> Option<&'a mut RigAxisRender> {
    if render.is_null() {
        None
    } else {
        Some(unsafe { &mut *render })
    }
}

fn blend_value(blend: Blend) -> u32 {
    match blend {
        Blend::Normal => 0,
        Blend::Additive => 1,
        Blend::Multiply => 2,
        Blend::Screen => 3,
    }
}

fn name_at(names: &[CString], index: u32) -> *const c_char {
    names
        .get(index as usize)
        .map_or(ptr::null(), |name| name.as_ptr())
}

#[no_mangle]
pub extern "C" fn rigaxis_abi_version() -> u32 {
    RIGAXIS_ABI_VERSION
}

#[no_mangle]
pub extern "C" fn rigaxis_version_string() -> *const c_char {
    VERSION.as_ptr() as *const c_char
}

#[no_mangle]
pub extern "C" fn rigaxis_last_error() -> *const c_char {
    LAST_ERROR.with(|slot| slot.borrow().as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_create(
    data: *const u8,
    size: usize,
    out: *mut *mut RigAxisSkeleton,
) -> RigAxisStatus {
    clear_error();
    if out.is_null() || (data.is_null() && size != 0) {
        set_error("invalid argument");
        return RigAxisStatus::InvalidArgument;
    }
    unsafe {
        *out = ptr::null_mut();
    }

    guard(|| {
        let bytes: &[u8] = if size == 0 {
            &[]
        } else {
            unsafe { slice::from_raw_parts(data, size) }
        };
        let skeleton = load_skeleton(bytes).map_err(|error| error.0)?;
        let format = c_string(skeleton.version.as_deref().unwrap_or("unknown"));
        let bone_names = skeleton
            .bones
            .iter()
            .map(|bone| c_string(&bone.name))
            .collect();
        let animation_names = skeleton
            .animations
            .iter()
            .map(|animation| c_string(&animation.name))
            .collect();
        let skin_names = skeleton
            .skins
            .iter()
            .map(|skin| c_string(&skin.name))
            .collect();
        let warnings = skeleton
            .warnings
            .iter()
            .map(|warning| c_string(warning))
            .collect();
        let handle = RigAxisSkeleton {
            data: skeleton,
            format,
            bone_names,
            animation_names,
            skin_names,
            warnings,
        };
        unsafe {
            *out = Box::into_raw(Box::new(handle));
        }
        Ok(())
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_free(skeleton: *mut RigAxisSkeleton) {
    if !skeleton.is_null() {
        unsafe {
            drop(Box::from_raw(skeleton));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_format(
    skeleton: *const RigAxisSkeleton,
) -> *const c_char {
    skeleton_ref(skeleton).map_or(ptr::null(), |skeleton| skeleton.format.as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_bone_count(skeleton: *const RigAxisSkeleton) -> u32 {
    skeleton_ref(skeleton).map_or(0, |skeleton| skeleton.bone_names.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_bone_name(
    skeleton: *const RigAxisSkeleton,
    index: u32,
) -> *const c_char {
    skeleton_ref(skeleton).map_or(ptr::null(), |skeleton| name_at(&skeleton.bone_names, index))
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_find_bone(
    skeleton: *const RigAxisSkeleton,
    name: *const c_char,
) -> i32 {
    let Some(skeleton) = skeleton_ref(skeleton) else {
        return -1;
    };
    if name.is_null() {
        return -1;
    }
    let Ok(name) = (unsafe { CStr::from_ptr(name) }).to_str() else {
        return -1;
    };
    skeleton
        .data
        .find_bone(name)
        .map_or(-1, |index| index as i32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_animation_count(skeleton: *const RigAxisSkeleton) -> u32 {
    skeleton_ref(skeleton).map_or(0, |skeleton| skeleton.animation_names.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_animation_name(
    skeleton: *const RigAxisSkeleton,
    index: u32,
) -> *const c_char {
    skeleton_ref(skeleton).map_or(ptr::null(), |skeleton| {
        name_at(&skeleton.animation_names, index)
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_find_animation(
    skeleton: *const RigAxisSkeleton,
    name: *const c_char,
) -> i32 {
    let Some(skeleton) = skeleton_ref(skeleton) else {
        return -1;
    };
    if name.is_null() {
        return -1;
    }
    let Ok(name) = (unsafe { CStr::from_ptr(name) }).to_str() else {
        return -1;
    };
    skeleton
        .data
        .find_animation(name)
        .map_or(-1, |index| index as i32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_skin_count(skeleton: *const RigAxisSkeleton) -> u32 {
    skeleton_ref(skeleton).map_or(0, |skeleton| skeleton.skin_names.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_skin_name(
    skeleton: *const RigAxisSkeleton,
    index: u32,
) -> *const c_char {
    skeleton_ref(skeleton).map_or(ptr::null(), |skeleton| name_at(&skeleton.skin_names, index))
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_warning_count(skeleton: *const RigAxisSkeleton) -> u32 {
    skeleton_ref(skeleton).map_or(0, |skeleton| skeleton.warnings.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_skeleton_warning_at(
    skeleton: *const RigAxisSkeleton,
    index: u32,
) -> *const c_char {
    skeleton_ref(skeleton).map_or(ptr::null(), |skeleton| name_at(&skeleton.warnings, index))
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_atlas_create(
    text: *const u8,
    size: usize,
    out: *mut *mut RigAxisAtlas,
) -> RigAxisStatus {
    clear_error();
    if out.is_null() || (text.is_null() && size != 0) {
        set_error("invalid argument");
        return RigAxisStatus::InvalidArgument;
    }
    unsafe {
        *out = ptr::null_mut();
    }

    guard(|| {
        let bytes: &[u8] = if size == 0 {
            &[]
        } else {
            unsafe { slice::from_raw_parts(text, size) }
        };
        let text =
            std::str::from_utf8(bytes).map_err(|error| format!("atlas is not UTF-8: {error}"))?;
        let atlas = Atlas::parse(text);
        let page_names = atlas
            .pages
            .iter()
            .map(|page| c_string(&page.name))
            .collect();
        let handle = RigAxisAtlas { atlas, page_names };
        unsafe {
            *out = Box::into_raw(Box::new(handle));
        }
        Ok(())
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_atlas_free(atlas: *mut RigAxisAtlas) {
    if !atlas.is_null() {
        unsafe {
            drop(Box::from_raw(atlas));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_atlas_page_count(atlas: *const RigAxisAtlas) -> u32 {
    atlas_ref(atlas).map_or(0, |atlas| atlas.page_names.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_atlas_page_name(
    atlas: *const RigAxisAtlas,
    index: u32,
) -> *const c_char {
    atlas_ref(atlas).map_or(ptr::null(), |atlas| name_at(&atlas.page_names, index))
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_lookup_create(
    skeleton: *const RigAxisSkeleton,
    atlas: *const RigAxisAtlas,
    out: *mut *mut RigAxisLookup,
) -> RigAxisStatus {
    clear_error();
    if skeleton.is_null() || atlas.is_null() || out.is_null() {
        set_error("invalid argument");
        return RigAxisStatus::InvalidArgument;
    }
    unsafe {
        *out = ptr::null_mut();
    }

    guard(|| {
        let skeleton = unsafe { &*skeleton };
        let atlas = unsafe { &*atlas };
        let lookup = RegionLookup::new(&skeleton.data, &atlas.atlas);
        let missing = lookup
            .missing(&skeleton.data)
            .into_iter()
            .map(|name| c_string(&name))
            .collect();
        let handle = RigAxisLookup { lookup, missing };
        unsafe {
            *out = Box::into_raw(Box::new(handle));
        }
        Ok(())
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_lookup_free(lookup: *mut RigAxisLookup) {
    if !lookup.is_null() {
        unsafe {
            drop(Box::from_raw(lookup));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_lookup_missing_count(lookup: *const RigAxisLookup) -> u32 {
    lookup_ref(lookup).map_or(0, |lookup| lookup.missing.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_lookup_missing_at(
    lookup: *const RigAxisLookup,
    index: u32,
) -> *const c_char {
    lookup_ref(lookup).map_or(ptr::null(), |lookup| name_at(&lookup.missing, index))
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_pose_create(
    skeleton: *const RigAxisSkeleton,
    out: *mut *mut RigAxisPose,
) -> RigAxisStatus {
    clear_error();
    if skeleton.is_null() || out.is_null() {
        set_error("invalid argument");
        return RigAxisStatus::InvalidArgument;
    }
    unsafe {
        *out = ptr::null_mut();
    }

    guard(|| {
        let skeleton = unsafe { &*skeleton };
        let pose = Pose::new(&skeleton.data);
        unsafe {
            *out = Box::into_raw(Box::new(RigAxisPose { pose }));
        }
        Ok(())
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_pose_free(pose: *mut RigAxisPose) {
    if !pose.is_null() {
        unsafe {
            drop(Box::from_raw(pose));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_pose_reset(
    skeleton: *const RigAxisSkeleton,
    pose: *mut RigAxisPose,
) {
    if skeleton.is_null() || pose.is_null() {
        return;
    }
    let skeleton = unsafe { &*skeleton };
    let pose = unsafe { &mut *pose };
    pose.pose.reset(&skeleton.data);
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_pose_update_world(
    skeleton: *const RigAxisSkeleton,
    pose: *mut RigAxisPose,
) {
    if skeleton.is_null() || pose.is_null() {
        return;
    }
    let skeleton = unsafe { &*skeleton };
    let pose = unsafe { &mut *pose };
    pose.pose.update_world(&skeleton.data);
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_pose_reset_physics(pose: *mut RigAxisPose) {
    if pose.is_null() {
        return;
    }
    let pose = unsafe { &mut *pose };
    pose.pose.reset_physics();
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_pose_set_skin(
    skeleton: *const RigAxisSkeleton,
    pose: *mut RigAxisPose,
    name: *const c_char,
) -> bool {
    if skeleton.is_null() || pose.is_null() || name.is_null() {
        return false;
    }
    let Ok(name) = (unsafe { CStr::from_ptr(name) }).to_str() else {
        return false;
    };
    let skeleton = unsafe { &*skeleton };
    let pose = unsafe { &mut *pose };
    let Some(skin) = skeleton.data.find_skin(name) else {
        return false;
    };
    pose.pose.skin = Some(skin);
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_pose_bone_world(
    pose: *const RigAxisPose,
    bone: u32,
    out: *mut RigAxisAffine,
) -> bool {
    let Some(pose) = pose_ref(pose) else {
        return false;
    };
    if out.is_null() {
        return false;
    }
    let Some(world) = pose.pose.world.get(bone as usize) else {
        return false;
    };
    unsafe {
        *out = RigAxisAffine {
            a: world.a,
            b: world.b,
            c: world.c,
            d: world.d,
            tx: world.tx,
            ty: world.ty,
        };
    }
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_create(out: *mut *mut RigAxisState) -> RigAxisStatus {
    clear_error();
    if out.is_null() {
        set_error("invalid argument");
        return RigAxisStatus::InvalidArgument;
    }
    unsafe {
        *out = ptr::null_mut();
    }

    guard(|| {
        let state = AnimationState::new();
        unsafe {
            *out = Box::into_raw(Box::new(RigAxisState {
                state,
                events: Vec::new(),
            }));
        }
        Ok(())
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_free(state: *mut RigAxisState) {
    if !state.is_null() {
        unsafe {
            drop(Box::from_raw(state));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_set_speed(state: *mut RigAxisState, speed: f64) {
    if state.is_null() {
        return;
    }
    let state = unsafe { &mut *state };
    state.state.speed = speed;
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_get_speed(state: *const RigAxisState) -> f64 {
    state_ref(state).map_or(0.0, |state| state.state.speed)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_play(
    state: *mut RigAxisState,
    skeleton: *const RigAxisSkeleton,
    name: *const c_char,
    looping: bool,
    mix: f64,
    out_entry: *mut u64,
) -> bool {
    if state.is_null() || skeleton.is_null() || name.is_null() {
        return false;
    }
    let Ok(name) = (unsafe { CStr::from_ptr(name) }).to_str() else {
        return false;
    };
    let state = unsafe { &mut *state };
    let skeleton = unsafe { &*skeleton };

    if !state.state.play(&skeleton.data, name, looping, mix) {
        return false;
    }
    if !out_entry.is_null() {
        if let Some(entry) = state.state.current(0) {
            unsafe {
                *out_entry = entry.id;
            }
        }
    }
    collect_events(state, skeleton);
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_set_animation(
    state: *mut RigAxisState,
    skeleton: *const RigAxisSkeleton,
    track: u32,
    name: *const c_char,
    looping: bool,
    mix: f64,
    out_entry: *mut u64,
) -> bool {
    if state.is_null() || skeleton.is_null() || name.is_null() {
        return false;
    }
    let Ok(name) = (unsafe { CStr::from_ptr(name) }).to_str() else {
        return false;
    };
    let state = unsafe { &mut *state };
    let skeleton = unsafe { &*skeleton };

    let Some(entry) = state
        .state
        .set_animation(&skeleton.data, track as usize, name, looping, mix)
    else {
        return false;
    };
    if !out_entry.is_null() {
        unsafe {
            *out_entry = entry;
        }
    }
    collect_events(state, skeleton);
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_add_animation(
    state: *mut RigAxisState,
    skeleton: *const RigAxisSkeleton,
    track: u32,
    name: *const c_char,
    looping: bool,
    mix: f64,
    delay: f64,
    has_delay: bool,
    out_entry: *mut u64,
) -> bool {
    if state.is_null() || skeleton.is_null() || name.is_null() {
        return false;
    }
    let Ok(name) = (unsafe { CStr::from_ptr(name) }).to_str() else {
        return false;
    };
    let state = unsafe { &mut *state };
    let skeleton = unsafe { &*skeleton };
    let delay = if has_delay { Some(delay) } else { None };

    let Some(entry) =
        state
            .state
            .add_animation(&skeleton.data, track as usize, name, looping, mix, delay)
    else {
        return false;
    };
    if !out_entry.is_null() {
        unsafe {
            *out_entry = entry;
        }
    }
    collect_events(state, skeleton);
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_clear_track(state: *mut RigAxisState, track: u32) {
    if state.is_null() {
        return;
    }
    let state = unsafe { &mut *state };
    state.state.clear_track(track as usize);
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_stop(state: *mut RigAxisState) {
    if state.is_null() {
        return;
    }
    let state = unsafe { &mut *state };
    state.state.stop();
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_set_track_alpha(
    state: *mut RigAxisState,
    track: u32,
    alpha: f64,
) -> bool {
    if state.is_null() {
        return false;
    }
    let state = unsafe { &mut *state };
    let Some(entry) = state.state.current_mut(track as usize) else {
        return false;
    };
    entry.alpha = alpha;
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_set_track_time_scale(
    state: *mut RigAxisState,
    track: u32,
    scale: f64,
) -> bool {
    if state.is_null() {
        return false;
    }
    let state = unsafe { &mut *state };
    let Some(entry) = state.state.current_mut(track as usize) else {
        return false;
    };
    entry.time_scale = scale;
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_set_track_time(
    state: *mut RigAxisState,
    track: u32,
    time: f64,
) -> bool {
    if state.is_null() {
        return false;
    }
    let state = unsafe { &mut *state };
    let Some(entry) = state.state.current_mut(track as usize) else {
        return false;
    };
    entry.time = time;
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_update(
    state: *mut RigAxisState,
    skeleton: *const RigAxisSkeleton,
    delta_seconds: f64,
) {
    if state.is_null() || skeleton.is_null() {
        return;
    }
    let state = unsafe { &mut *state };
    let skeleton = unsafe { &*skeleton };
    state.state.update(&skeleton.data, delta_seconds);
    collect_events(state, skeleton);
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_apply(
    state: *mut RigAxisState,
    skeleton: *const RigAxisSkeleton,
    pose: *mut RigAxisPose,
) {
    if state.is_null() || skeleton.is_null() || pose.is_null() {
        return;
    }
    let state = unsafe { &mut *state };
    let skeleton = unsafe { &*skeleton };
    let pose = unsafe { &mut *pose };
    state.state.apply(&skeleton.data, &mut pose.pose);
    collect_events(state, skeleton);
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_event_count(state: *const RigAxisState) -> u32 {
    state_ref(state).map_or(0, |state| state.events.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_event_at(
    state: *const RigAxisState,
    index: u32,
    out: *mut RigAxisEvent,
) -> bool {
    let Some(state) = state_ref(state) else {
        return false;
    };
    if out.is_null() {
        return false;
    }
    let Some(event) = state.events.get(index as usize) else {
        return false;
    };
    unsafe {
        *out = RigAxisEvent {
            kind: event.kind,
            track: event.track,
            animation: event.animation,
            entry: event.entry,
            animation_name: event.animation_name.as_ptr(),
            event_name: event
                .event_name
                .as_ref()
                .map_or(ptr::null(), |name| name.as_ptr()),
            time: event.time,
            loop_index: event.loop_index,
            int_value: event.int_value,
            float_value: event.float_value,
            string_value: event
                .string_value
                .as_ref()
                .map_or(ptr::null(), |value| value.as_ptr()),
            audio: event
                .audio
                .as_ref()
                .map_or(ptr::null(), |value| value.as_ptr()),
            volume: event.volume,
            balance: event.balance,
        };
    }
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_state_clear_events(state: *mut RigAxisState) {
    if state.is_null() {
        return;
    }
    let state = unsafe { &mut *state };
    state.events.clear();
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_render_create(out: *mut *mut RigAxisRender) -> RigAxisStatus {
    clear_error();
    if out.is_null() {
        set_error("invalid argument");
        return RigAxisStatus::InvalidArgument;
    }
    unsafe {
        *out = ptr::null_mut();
    }

    guard(|| {
        let render = RigAxisRender {
            output: RenderOutput::default(),
            batches: Vec::new(),
        };
        unsafe {
            *out = Box::into_raw(Box::new(render));
        }
        Ok(())
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_render_free(render: *mut RigAxisRender) {
    if !render.is_null() {
        unsafe {
            drop(Box::from_raw(render));
        }
    }
}

fn update_batch_views(render: &mut RigAxisRender) {
    let RigAxisRender { output, batches } = render;
    let native_batches = output.batches();
    if batches.len() != native_batches.len() {
        batches.resize_with(native_batches.len(), RenderBatchView::default);
    }
    for (view, batch) in batches.iter_mut().zip(native_batches) {
        view.source = batch.source as u32;
        view.page = batch.page as u32;
        view.blend = blend_value(batch.blend);
        view.vertices.clear();
        view.indices.clear();
        view.indices.extend_from_slice(&batch.indices);

        for index in 0..batch.positions.len() {
            let position = batch.positions[index];
            let uv = batch.uvs[index];
            let light = batch.colors[index];
            let dark = batch.darks[index];
            view.vertices.push(RigAxisVertex {
                x: position[0],
                y: position[1],
                u: uv[0],
                v: uv[1],
                light_r: light[0],
                light_g: light[1],
                light_b: light[2],
                light_a: light[3],
                dark_r: dark[0],
                dark_g: dark[1],
                dark_b: dark[2],
                dark_a: dark[3],
            });
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_render_build(
    render: *mut RigAxisRender,
    skeleton: *const RigAxisSkeleton,
    pose: *const RigAxisPose,
    lookup: *const RigAxisLookup,
    tint: *const f32,
) -> RigAxisStatus {
    clear_error();
    if render.is_null()
        || skeleton.is_null()
        || pose.is_null()
        || lookup.is_null()
        || tint.is_null()
    {
        set_error("invalid argument");
        return RigAxisStatus::InvalidArgument;
    }

    guard(|| {
        let render = unsafe { &mut *render };
        let skeleton = unsafe { &*skeleton };
        let pose = unsafe { &*pose };
        let lookup = unsafe { &*lookup };
        let tint = unsafe { slice::from_raw_parts(tint, 4) };
        let tint = [
            tint[0] as f64,
            tint[1] as f64,
            tint[2] as f64,
            tint[3] as f64,
        ];

        render.output.build(&skeleton.data, &pose.pose, &lookup.lookup, tint);
        update_batch_views(render);
        Ok(())
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_render_batch_count(render: *const RigAxisRender) -> u32 {
    render_ref(render).map_or(0, |render| render.batches.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_render_batch_at(
    render: *const RigAxisRender,
    index: u32,
    out: *mut RigAxisBatch,
) -> bool {
    let Some(render) = render_ref(render) else {
        return false;
    };
    if out.is_null() {
        return false;
    }
    let Some(batch) = render.batches.get(index as usize) else {
        return false;
    };
    unsafe {
        *out = RigAxisBatch {
            source: batch.source,
            page: batch.page,
            blend: batch.blend,
            vertex_count: batch.vertices.len() as u32,
            index_count: batch.indices.len() as u32,
            vertices: batch.vertices.as_ptr(),
            indices: batch.indices.as_ptr(),
        };
    }
    true
}

fn atlas_from_bytes(bytes: &[u8]) -> Result<Atlas, String> {
    let text = std::str::from_utf8(bytes).map_err(|error| format!("atlas is not UTF-8: {error}"))?;
    Ok(Atlas::parse(text))
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_create(
    manifest: *const u8,
    manifest_size: usize,
    sources: *const RigAxisSceneSource,
    source_count: u32,
    out: *mut *mut RigAxisScene,
) -> RigAxisStatus {
    clear_error();
    if out.is_null()
        || (manifest.is_null() && manifest_size != 0)
        || (sources.is_null() && source_count != 0)
    {
        set_error("invalid argument");
        return RigAxisStatus::InvalidArgument;
    }
    unsafe {
        *out = ptr::null_mut();
    }

    guard(|| {
        let manifest_bytes: &[u8] = if manifest_size == 0 {
            &[]
        } else {
            unsafe { slice::from_raw_parts(manifest, manifest_size) }
        };

        let mut files: HashMap<String, (Vec<u8>, Vec<u8>)> = HashMap::new();
        for index in 0..source_count as usize {
            let source = unsafe { &*sources.add(index) };
            if source.path.is_null()
                || (source.skeleton.is_null() && source.skeleton_size != 0)
                || (source.atlas.is_null() && source.atlas_size != 0)
            {
                return Err("invalid scene source".to_string());
            }
            let path = unsafe { CStr::from_ptr(source.path) }
                .to_string_lossy()
                .to_string();
            let skeleton = if source.skeleton_size == 0 {
                Vec::new()
            } else {
                unsafe { slice::from_raw_parts(source.skeleton, source.skeleton_size) }.to_vec()
            };
            let atlas = if source.atlas_size == 0 {
                Vec::new()
            } else {
                unsafe { slice::from_raw_parts(source.atlas, source.atlas_size) }.to_vec()
            };
            files.insert(path, (skeleton, atlas));
        }

        let mut player = load_scene(manifest_bytes, |file| {
            files
                .get(file)
                .map(|(skeleton, _)| skeleton.clone())
                .ok_or_else(|| format!("missing scene source '{file}'"))
        })
        .map_err(|error| error.0)?;

        let scene_manifest = read_scene_manifest(manifest_bytes).map_err(|error| error.0)?;
        let (_, stage_atlas) = files
            .get(&scene_manifest.stage)
            .ok_or_else(|| format!("missing scene source '{}'", scene_manifest.stage))?;
        let stage_lookup = RegionLookup::new(&player.stage, &atlas_from_bytes(stage_atlas)?);

        let mut source_ids = Vec::with_capacity(player.characters.len() + 1);
        source_ids.push(c_string(""));
        let mut character_lookups = Vec::with_capacity(player.characters.len());
        for character in &player.characters {
            let actor = scene_manifest
                .scene
                .actors
                .iter()
                .find(|actor| actor.id == character.id)
                .ok_or_else(|| format!("scene actor '{}' not found", character.id))?;
            let path = actor
                .skeleton
                .as_ref()
                .ok_or_else(|| format!("scene actor '{}' has no skeleton", actor.id))?;
            let (_, atlas) = files
                .get(path)
                .ok_or_else(|| format!("missing scene source '{path}'"))?;
            character_lookups.push(RegionLookup::new(&character.data, &atlas_from_bytes(atlas)?));
            source_ids.push(c_string(&character.id));
        }

        let actor_ids = scene_manifest
            .scene
            .actors
            .iter()
            .map(|actor| c_string(&actor.id))
            .collect();
        let actor_names = scene_manifest
            .scene
            .actors
            .iter()
            .map(|actor| c_string(&actor.name))
            .collect();
        let current_shot = c_string(player.shot.as_deref().unwrap_or(""));
        player.evaluate(0.0);

        unsafe {
            *out = Box::into_raw(Box::new(RigAxisScene {
                player,
                stage_lookup,
                character_lookups,
                source_ids,
                actor_ids,
                actor_names,
                current_shot,
            }));
        }
        Ok(())
    })
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_free(scene: *mut RigAxisScene) {
    if !scene.is_null() {
        drop(unsafe { Box::from_raw(scene) });
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_play(scene: *mut RigAxisScene, shot: *const c_char, looping: bool) {
    clear_error();
    if shot.is_null() {
        set_error("invalid argument");
        return;
    }
    let Ok(name) = unsafe { CStr::from_ptr(shot) }.to_str() else {
        set_error("shot name is not UTF-8");
        return;
    };
    if let Some(scene) = scene_mut(scene) {
        scene.player.play(name, looping);
        scene.current_shot = c_string(scene.player.shot.as_deref().unwrap_or(""));
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_update(scene: *mut RigAxisScene, delta_seconds: f64) {
    if let Some(scene) = scene_mut(scene) {
        scene.player.update(delta_seconds.max(0.0));
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_set_time(
    scene: *mut RigAxisScene,
    time: f64,
    physics_delta: f64,
) {
    if let Some(scene) = scene_mut(scene) {
        scene.player.time = time.max(0.0);
        scene.player.evaluate(physics_delta.max(0.0));
    }
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_duration(scene: *const RigAxisScene) -> f64 {
    scene_ref(scene).map_or(0.0, |scene| scene.player.duration())
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_time(scene: *const RigAxisScene) -> f64 {
    scene_ref(scene).map_or(0.0, |scene| scene.player.time)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_current_shot(scene: *const RigAxisScene) -> *const c_char {
    scene_ref(scene).map_or(ptr::null(), |scene| scene.current_shot.as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_source_count(scene: *const RigAxisScene) -> u32 {
    scene_ref(scene).map_or(0, |scene| scene.source_ids.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_source_id(
    scene: *const RigAxisScene,
    source: u32,
) -> *const c_char {
    scene_ref(scene)
        .and_then(|scene| scene.source_ids.get(source as usize))
        .map_or(ptr::null(), |id| id.as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_actor_count(scene: *const RigAxisScene) -> u32 {
    scene_ref(scene).map_or(0, |scene| scene.player.scene.actors.len() as u32)
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_actor_id(
    scene: *const RigAxisScene,
    actor: u32,
) -> *const c_char {
    scene_ref(scene)
        .and_then(|scene| scene.actor_ids.get(actor as usize))
        .map_or(ptr::null(), |id| id.as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_actor_name(
    scene: *const RigAxisScene,
    actor: u32,
) -> *const c_char {
    scene_ref(scene)
        .and_then(|scene| scene.actor_names.get(actor as usize))
        .map_or(ptr::null(), |name| name.as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_actor_bone_world(
    scene: *const RigAxisScene,
    actor: u32,
    bone: *const c_char,
    out: *mut RigAxisAffine,
) -> bool {
    let Some(scene) = scene_ref(scene) else {
        return false;
    };
    if bone.is_null() || out.is_null() {
        return false;
    }
    let Ok(bone) = unsafe { CStr::from_ptr(bone) }.to_str() else {
        return false;
    };
    let Some(character) = scene.player.character(actor as usize) else {
        return false;
    };
    let Some(index) = character.data.find_bone(bone) else {
        return false;
    };
    let Some(world) = character.pose.world.get(index) else {
        return false;
    };
    unsafe {
        *out = affine_value(world);
    }
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_camera_world(
    scene: *const RigAxisScene,
    out: *mut RigAxisAffine,
) -> bool {
    let Some(scene) = scene_ref(scene) else {
        return false;
    };
    if out.is_null() {
        return false;
    }
    let Some(index) = scene.player.stage.find_bone("camera") else {
        return false;
    };
    let Some(world) = scene.player.pose.world.get(index) else {
        return false;
    };
    unsafe {
        *out = affine_value(world);
    }
    true
}

#[no_mangle]
pub unsafe extern "C" fn rigaxis_scene_build_render(scene: *mut RigAxisScene, render: *mut RigAxisRender) {
    clear_error();
    let Some(scene) = scene_mut(scene) else {
        set_error("invalid scene");
        return;
    };
    let Some(render) = render_mut(render) else {
        set_error("invalid render");
        return;
    };

    render.output.clear();
    let layers = scene.player.draw_list();
    let mut index = 0;
    while index < layers.len() {
        if layers[index].actor.is_none() {
            let start = index;
            while index < layers.len() && layers[index].actor.is_none() {
                index += 1;
            }
            let slots: Vec<usize> = layers[start..index].iter().map(|layer| layer.slot).collect();
            render.output.append(
                &scene.player.stage,
                &scene.player.pose,
                &scene.stage_lookup,
                [1.0; 4],
                &slots,
                0,
            );
        } else {
            let layer = layers[index];
            if let Some(character) = scene.player.character(layer.actor.unwrap()) {
                if let Some(character_index) = scene
                    .player
                    .characters
                    .iter()
                    .position(|candidate| candidate.id == character.id)
                {
                    let slots = character.pose.draw_order.clone();
                    render.output.append(
                        &character.data,
                        &character.pose,
                        &scene.character_lookups[character_index],
                        layer.tint,
                        &slots,
                        character_index + 1,
                    );
                }
            }
            index += 1;
        }
    }
    update_batch_views(render);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::{CStr, CString};

    const SKELETON: &str = r#"{"skeleton":{"spine":"4.3"},"bones":[{"name":"root"},{"name":"child","parent":"root","x":10}],"slots":[],"skins":[],"animations":{}}"#;
    const ATLAS: &str =
        "minimal.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\n";
    const ANIMATED_SKELETON: &str = r#"{
      "skeleton": {"spine": "4.3"},
      "bones": [
        {"name": "root"},
        {"name": "child", "parent": "root", "x": 10}
      ],
      "slots": [],
      "skins": [],
      "animations": {
        "move": {
          "bones": {
            "child": {
              "translate": [
                {"time": 0, "x": 0, "y": 0},
                {"time": 1, "x": 10, "y": 0}
              ]
            }
          }
        },
        "move2": {
          "bones": {
            "child": {
              "translate": [
                {"time": 0, "x": 0, "y": 0},
                {"time": 1, "x": 100, "y": 0}
              ]
            }
          }
        }
      }
    }"#;
    const RENDER_SKELETON: &str = r#"{
      "skeleton": {"spine": "4.3"},
      "bones": [
        {"name": "root"},
        {"name": "bone", "parent": "root"}
      ],
      "slots": [
        {"name": "slot", "bone": "bone", "attachment": "image"}
      ],
      "skins": [
        {
          "name": "default",
          "attachments": {
            "slot": {
              "image": {"type": "region", "x": 0, "y": 0, "width": 32, "height": 32}
            }
          }
        }
      ],
      "animations": {}
    }"#;
    const RENDER_ATLAS: &str = "render.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\nimage\n  bounds: 0,0,32,32\n";
    const EVENT_SKELETON: &str = r#"{
      "skeleton": {"spine": "4.3"},
      "bones": [{"name": "root"}],
      "slots": [],
      "skins": [],
      "events": {
        "footstep": {"int": 7, "float": 2.5, "string": "step", "audio": "step.wav", "volume": 0.8, "balance": -0.2}
      },
      "animations": {
        "walk": {
          "events": [
            {"time": 0.5, "name": "footstep"},
            {"time": 1.0, "name": "done", "int": 9}
          ]
        }
      }
    }"#;

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
            volume: 1.0,
            balance: 0.0,
        }
    }

    #[test]
    fn loads_skeleton_and_atlas() {
        unsafe {
            let mut skeleton: *mut RigAxisSkeleton = ptr::null_mut();
            let status = rigaxis_skeleton_create(SKELETON.as_ptr(), SKELETON.len(), &mut skeleton);
            assert_eq!(status as i32, RigAxisStatus::Ok as i32);
            assert!(!skeleton.is_null());

            assert_eq!(rigaxis_skeleton_bone_count(skeleton), 2);
            let name = CStr::from_ptr(rigaxis_skeleton_bone_name(skeleton, 1))
                .to_str()
                .unwrap();
            assert_eq!(name, "child");
            let root = CString::new("root").unwrap();
            assert_eq!(rigaxis_skeleton_find_bone(skeleton, root.as_ptr()), 0);

            let mut atlas: *mut RigAxisAtlas = ptr::null_mut();
            let status = rigaxis_atlas_create(ATLAS.as_ptr(), ATLAS.len(), &mut atlas);
            assert_eq!(status as i32, RigAxisStatus::Ok as i32);
            assert!(!atlas.is_null());

            assert_eq!(rigaxis_atlas_page_count(atlas), 1);
            let page = CStr::from_ptr(rigaxis_atlas_page_name(atlas, 0))
                .to_str()
                .unwrap();
            assert_eq!(page, "minimal.png");

            let mut lookup: *mut RigAxisLookup = ptr::null_mut();
            let status = rigaxis_lookup_create(skeleton, atlas, &mut lookup);
            assert_eq!(status as i32, RigAxisStatus::Ok as i32);
            assert!(!lookup.is_null());

            assert_eq!(rigaxis_lookup_missing_count(lookup), 0);
            rigaxis_lookup_free(lookup);
            rigaxis_atlas_free(atlas);
            rigaxis_skeleton_free(skeleton);
        }
    }

    #[test]
    fn reports_invalid_skeleton() {
        unsafe {
            let data = b"not a skeleton";
            let mut skeleton: *mut RigAxisSkeleton = ptr::null_mut();
            let status = rigaxis_skeleton_create(data.as_ptr(), data.len(), &mut skeleton);
            assert_eq!(status as i32, RigAxisStatus::InvalidData as i32);
            assert!(skeleton.is_null());
            let error = CStr::from_ptr(rigaxis_last_error()).to_str().unwrap();
            assert!(!error.is_empty());
        }
    }

    #[test]
    fn plays_animation_and_poses() {
        unsafe {
            let mut skeleton: *mut RigAxisSkeleton = ptr::null_mut();
            let status = rigaxis_skeleton_create(
                ANIMATED_SKELETON.as_ptr(),
                ANIMATED_SKELETON.len(),
                &mut skeleton,
            );
            assert_eq!(status as i32, RigAxisStatus::Ok as i32);

            let mut pose: *mut RigAxisPose = ptr::null_mut();
            assert_eq!(
                rigaxis_pose_create(skeleton, &mut pose) as i32,
                RigAxisStatus::Ok as i32
            );
            let mut state: *mut RigAxisState = ptr::null_mut();
            assert_eq!(
                rigaxis_state_create(&mut state) as i32,
                RigAxisStatus::Ok as i32
            );

            let name = CString::new("move").unwrap();
            assert!(rigaxis_state_play(
                state,
                skeleton,
                name.as_ptr(),
                false,
                0.0,
                ptr::null_mut()
            ));
            rigaxis_state_update(state, skeleton, 0.5);
            rigaxis_state_apply(state, skeleton, pose);

            let mut affine = RigAxisAffine {
                a: 0.0,
                b: 0.0,
                c: 0.0,
                d: 0.0,
                tx: 0.0,
                ty: 0.0,
            };
            assert!(rigaxis_pose_bone_world(pose, 1, &mut affine));
            assert!((affine.tx - 15.0).abs() < 1e-9);
            assert!(affine.ty.abs() < 1e-9);
            assert!((affine.a - 1.0).abs() < 1e-9);
            assert!(affine.b.abs() < 1e-9);
            assert!(affine.c.abs() < 1e-9);
            assert!((affine.d - 1.0).abs() < 1e-9);

            rigaxis_state_free(state);
            rigaxis_pose_free(pose);
            rigaxis_skeleton_free(skeleton);
        }
    }

    #[test]
    fn crossfades_and_queues() {
        unsafe {
            let mut skeleton: *mut RigAxisSkeleton = ptr::null_mut();
            let status = rigaxis_skeleton_create(
                ANIMATED_SKELETON.as_ptr(),
                ANIMATED_SKELETON.len(),
                &mut skeleton,
            );
            assert_eq!(status as i32, RigAxisStatus::Ok as i32);

            let mut pose: *mut RigAxisPose = ptr::null_mut();
            assert_eq!(
                rigaxis_pose_create(skeleton, &mut pose) as i32,
                RigAxisStatus::Ok as i32
            );
            let mut state: *mut RigAxisState = ptr::null_mut();
            assert_eq!(
                rigaxis_state_create(&mut state) as i32,
                RigAxisStatus::Ok as i32
            );

            let move_name = CString::new("move").unwrap();
            let move2_name = CString::new("move2").unwrap();
            let mut affine = RigAxisAffine {
                a: 0.0,
                b: 0.0,
                c: 0.0,
                d: 0.0,
                tx: 0.0,
                ty: 0.0,
            };

            assert!(rigaxis_state_play(
                state,
                skeleton,
                move_name.as_ptr(),
                false,
                0.0,
                ptr::null_mut()
            ));
            rigaxis_state_update(state, skeleton, 0.25);
            assert!(rigaxis_state_set_animation(
                state,
                skeleton,
                0,
                move2_name.as_ptr(),
                false,
                0.5,
                ptr::null_mut()
            ));
            rigaxis_state_update(state, skeleton, 0.1);
            rigaxis_state_apply(state, skeleton, pose);
            assert!(rigaxis_pose_bone_world(pose, 1, &mut affine));
            assert!((affine.tx - 14.8).abs() < 1e-9);

            rigaxis_state_stop(state);
            assert!(rigaxis_state_play(
                state,
                skeleton,
                move_name.as_ptr(),
                false,
                0.0,
                ptr::null_mut()
            ));
            assert!(rigaxis_state_add_animation(
                state,
                skeleton,
                0,
                move2_name.as_ptr(),
                false,
                0.0,
                0.0,
                false,
                ptr::null_mut()
            ));
            rigaxis_state_update(state, skeleton, 1.1);
            rigaxis_state_apply(state, skeleton, pose);
            assert!(rigaxis_pose_bone_world(pose, 1, &mut affine));
            assert!((affine.tx - 20.0).abs() < 1e-9);

            rigaxis_state_free(state);
            rigaxis_pose_free(pose);
            rigaxis_skeleton_free(skeleton);
        }
    }

    #[test]
    fn builds_render_batches() {
        unsafe {
            let mut skeleton: *mut RigAxisSkeleton = ptr::null_mut();
            assert_eq!(
                rigaxis_skeleton_create(
                    RENDER_SKELETON.as_ptr(),
                    RENDER_SKELETON.len(),
                    &mut skeleton
                ) as i32,
                RigAxisStatus::Ok as i32
            );
            let mut atlas: *mut RigAxisAtlas = ptr::null_mut();
            assert_eq!(
                rigaxis_atlas_create(RENDER_ATLAS.as_ptr(), RENDER_ATLAS.len(), &mut atlas) as i32,
                RigAxisStatus::Ok as i32
            );
            let mut lookup: *mut RigAxisLookup = ptr::null_mut();
            assert_eq!(
                rigaxis_lookup_create(skeleton, atlas, &mut lookup) as i32,
                RigAxisStatus::Ok as i32
            );
            let mut pose: *mut RigAxisPose = ptr::null_mut();
            assert_eq!(
                rigaxis_pose_create(skeleton, &mut pose) as i32,
                RigAxisStatus::Ok as i32
            );
            rigaxis_pose_reset(skeleton, pose);
            rigaxis_pose_update_world(skeleton, pose);

            let mut render: *mut RigAxisRender = ptr::null_mut();
            assert_eq!(
                rigaxis_render_create(&mut render) as i32,
                RigAxisStatus::Ok as i32
            );

            let tint = [1.0f32, 1.0, 1.0, 1.0];
            assert_eq!(
                rigaxis_render_build(render, skeleton, pose, lookup, tint.as_ptr()) as i32,
                RigAxisStatus::Ok as i32
            );
            assert_eq!(rigaxis_render_batch_count(render), 1);

            let mut batch = RigAxisBatch {
                source: 0,
                page: 0,
                blend: 0,
                vertex_count: 0,
                index_count: 0,
                vertices: ptr::null(),
                indices: ptr::null(),
            };
            assert!(rigaxis_render_batch_at(render, 0, &mut batch));
            assert_eq!(batch.page, 0);
            assert_eq!(batch.blend, 0);
            assert_eq!(batch.vertex_count, 4);
            assert_eq!(batch.index_count, 6);

            let vertices = std::slice::from_raw_parts(batch.vertices, 4);
            let indices = std::slice::from_raw_parts(batch.indices, 6);
            assert_eq!(indices, &[0, 1, 2, 0, 2, 3]);
            for vertex in vertices {
                assert!(vertex.x.abs() <= 16.0 + 1e-5);
                assert!(vertex.y.abs() <= 16.0 + 1e-5);
                assert!((0.0..=1.0).contains(&vertex.u));
                assert!((0.0..=1.0).contains(&vertex.v));
                assert!((vertex.light_r - 1.0).abs() < 1e-6);
            }

            let tint = [0.5f32, 0.5, 0.5, 1.0];
            assert_eq!(
                rigaxis_render_build(render, skeleton, pose, lookup, tint.as_ptr()) as i32,
                RigAxisStatus::Ok as i32
            );
            assert!(rigaxis_render_batch_at(render, 0, &mut batch));
            let vertices = std::slice::from_raw_parts(batch.vertices, 4);
            assert!((vertices[0].light_r - 0.5).abs() < 1e-6);

            rigaxis_render_free(render);
            rigaxis_pose_free(pose);
            rigaxis_lookup_free(lookup);
            rigaxis_atlas_free(atlas);
            rigaxis_skeleton_free(skeleton);
        }
    }

    #[test]
    fn exposes_events() {
        unsafe {
            let mut skeleton: *mut RigAxisSkeleton = ptr::null_mut();
            assert_eq!(
                rigaxis_skeleton_create(
                    EVENT_SKELETON.as_ptr(),
                    EVENT_SKELETON.len(),
                    &mut skeleton
                ) as i32,
                RigAxisStatus::Ok as i32
            );
            let mut pose: *mut RigAxisPose = ptr::null_mut();
            assert_eq!(
                rigaxis_pose_create(skeleton, &mut pose) as i32,
                RigAxisStatus::Ok as i32
            );
            let mut state: *mut RigAxisState = ptr::null_mut();
            assert_eq!(
                rigaxis_state_create(&mut state) as i32,
                RigAxisStatus::Ok as i32
            );

            let walk = CString::new("walk").unwrap();
            assert!(rigaxis_state_play(
                state,
                skeleton,
                walk.as_ptr(),
                false,
                0.0,
                ptr::null_mut()
            ));
            assert_eq!(rigaxis_state_event_count(state), 1);

            let mut event = empty_event();
            assert!(rigaxis_state_event_at(state, 0, &mut event));
            assert_eq!(event.kind, 0);
            assert_eq!(
                CStr::from_ptr(event.animation_name).to_str().unwrap(),
                "walk"
            );
            assert!(event.event_name.is_null());

            rigaxis_state_update(state, skeleton, 0.6);
            assert_eq!(rigaxis_state_event_count(state), 2);
            assert!(rigaxis_state_event_at(state, 1, &mut event));
            assert_eq!(event.kind, 3);
            assert_eq!(
                CStr::from_ptr(event.event_name).to_str().unwrap(),
                "footstep"
            );
            assert_eq!(event.int_value, 7);
            assert!((event.float_value - 2.5).abs() < 1e-9);
            assert_eq!(CStr::from_ptr(event.string_value).to_str().unwrap(), "step");
            assert_eq!(CStr::from_ptr(event.audio).to_str().unwrap(), "step.wav");
            assert!((event.volume - 0.8).abs() < 1e-9);
            assert!((event.balance - -0.2).abs() < 1e-9);
            assert!((event.time - 0.5).abs() < 1e-9);

            rigaxis_state_update(state, skeleton, 0.5);
            assert_eq!(rigaxis_state_event_count(state), 4);
            assert!(rigaxis_state_event_at(state, 2, &mut event));
            assert_eq!(event.kind, 3);
            assert_eq!(CStr::from_ptr(event.event_name).to_str().unwrap(), "done");
            assert_eq!(event.int_value, 9);
            assert!(rigaxis_state_event_at(state, 3, &mut event));
            assert_eq!(event.kind, 2);

            rigaxis_state_clear_events(state);
            assert_eq!(rigaxis_state_event_count(state), 0);

            rigaxis_state_free(state);
            rigaxis_pose_free(pose);
            rigaxis_skeleton_free(skeleton);
        }
    }

    #[test]
    fn builds_scene_render() {
        const MANIFEST: &str = r#"{
          "rigbyScene": 1.0,
          "stage": "stage.json",
          "actors": [{
            "id": "hero",
            "name": "Hero",
            "bone": "hero",
            "slot": "hero",
            "skeleton": "hero.json",
            "clips": {"shot": [{"animation": "idle"}]}
          }]
        }"#;
        const STAGE: &str = r#"{
          "skeleton": {"spine": "4.3"},
          "bones": [
            {"name": "root"},
            {"name": "camera", "parent": "root"},
            {"name": "hero", "parent": "root"}
          ],
          "slots": [{"name": "hero", "bone": "hero"}],
          "skins": [],
          "animations": {
            "shot": {
              "bones": {
                "camera": {
                  "translate": [
                    {"time": 0, "x": 0, "y": 0},
                    {"time": 1, "x": 10, "y": 0}
                  ]
                }
              }
            }
          }
        }"#;
        const HERO: &str = r#"{
          "skeleton": {"spine": "4.3"},
          "bones": [{"name": "root"}],
          "slots": [{"name": "body", "bone": "root", "attachment": "body"}],
          "skins": [{
            "name": "default",
            "attachments": {
              "body": {
                "body": {"type": "region", "x": 0, "y": 0, "width": 32, "height": 32}
              }
            }
          }],
          "animations": {"idle": {"bones": {"root": {"translate": [{"time": 0, "x": 0, "y": 0}]}}}}
        }"#;
        const ATLAS: &str =
            "atlas.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\nbody\n  bounds: 0,0,32,32\n";

        unsafe {
            let manifest = MANIFEST.as_bytes();
            let stage = CString::new(STAGE).unwrap();
            let hero = CString::new(HERO).unwrap();
            let atlas = CString::new(ATLAS).unwrap();
            let stage_path = CString::new("stage.json").unwrap();
            let hero_path = CString::new("hero.json").unwrap();
            let sources = [
                RigAxisSceneSource {
                    path: stage_path.as_ptr(),
                    skeleton: stage.as_ptr() as *const u8,
                    skeleton_size: stage.as_bytes().len(),
                    atlas: atlas.as_ptr() as *const u8,
                    atlas_size: atlas.as_bytes().len(),
                },
                RigAxisSceneSource {
                    path: hero_path.as_ptr(),
                    skeleton: hero.as_ptr() as *const u8,
                    skeleton_size: hero.as_bytes().len(),
                    atlas: atlas.as_ptr() as *const u8,
                    atlas_size: atlas.as_bytes().len(),
                },
            ];

            let mut scene: *mut RigAxisScene = std::ptr::null_mut();
            let status = rigaxis_scene_create(
                manifest.as_ptr(),
                manifest.len(),
                sources.as_ptr(),
                sources.len() as u32,
                &mut scene,
            );
            assert_eq!(status as i32, RigAxisStatus::Ok as i32);
            assert!(!scene.is_null());

            assert_eq!(rigaxis_scene_source_count(scene), 2);
            assert_eq!(rigaxis_scene_actor_count(scene), 1);
            assert_eq!(
                CStr::from_ptr(rigaxis_scene_actor_id(scene, 0)).to_str().unwrap(),
                "hero"
            );
            assert_eq!(
                CStr::from_ptr(rigaxis_scene_actor_name(scene, 0)).to_str().unwrap(),
                "Hero"
            );
            assert_eq!(
                CStr::from_ptr(rigaxis_scene_source_id(scene, 1)).to_str().unwrap(),
                "hero"
            );

            let shot = CString::new("shot").unwrap();
            rigaxis_scene_play(scene, shot.as_ptr(), true);
            rigaxis_scene_update(scene, 0.5);
            assert_eq!(
                CStr::from_ptr(rigaxis_scene_current_shot(scene)).to_str().unwrap(),
                "shot"
            );
            assert!((rigaxis_scene_time(scene) - 0.5).abs() < 1e-9);
            assert!(rigaxis_scene_duration(scene) >= 1.0);

            let mut camera = RigAxisAffine {
                a: 0.0,
                b: 0.0,
                c: 0.0,
                d: 0.0,
                tx: 0.0,
                ty: 0.0,
            };
            assert!(rigaxis_scene_camera_world(scene, &mut camera));
            assert!((camera.tx - 5.0).abs() < 1e-9);

            let root = CString::new("root").unwrap();
            let mut bone = camera;
            assert!(rigaxis_scene_actor_bone_world(
                scene,
                0,
                root.as_ptr(),
                &mut bone
            ));

            let mut render: *mut RigAxisRender = std::ptr::null_mut();
            assert_eq!(
                rigaxis_render_create(&mut render) as i32,
                RigAxisStatus::Ok as i32
            );
            rigaxis_scene_build_render(scene, render);
            assert!(rigaxis_render_batch_count(render) >= 1);

            let mut batch = RigAxisBatch {
                source: 0,
                page: 0,
                blend: 0,
                vertex_count: 0,
                index_count: 0,
                vertices: std::ptr::null(),
                indices: std::ptr::null(),
            };
            assert!(rigaxis_render_batch_at(render, 0, &mut batch));
            assert_eq!(batch.source, 1);
            assert!(batch.vertex_count > 0);
            assert!(batch.index_count > 0);

            rigaxis_render_free(render);
            rigaxis_scene_free(scene);
        }
    }
}
