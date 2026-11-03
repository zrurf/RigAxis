//! C ABI for the Rigby2D runtime used by the RigAxis Unreal Engine plugin.
//!
//! Handles are opaque. Strings are UTF-8, null-terminated, and stay valid until the owning
//! handle is mutated or freed. Callers must use a single thread per handle. Functions that take
//! a handle require a live handle from the matching `*_create` call; freeing it invalidates it.

#![allow(clippy::missing_safety_doc)]

use std::cell::RefCell;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::slice;

use rigby_runtime::{load_skeleton, AnimationState, Atlas, Pose, RegionLookup, SkeletonData};

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
    #[allow(dead_code)]
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
            *out = Box::into_raw(Box::new(RigAxisState { state }));
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
    // Events are exposed by a later milestone.
    state.state.drain_events();
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
}
