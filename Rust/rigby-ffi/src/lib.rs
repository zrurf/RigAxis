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

use rigby_runtime::{load_skeleton, Atlas, RegionLookup, SkeletonData};

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

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::{CStr, CString};

    const SKELETON: &str = r#"{"skeleton":{"spine":"4.3"},"bones":[{"name":"root"},{"name":"child","parent":"root","x":10}],"slots":[],"skins":[],"animations":{}}"#;
    const ATLAS: &str =
        "minimal.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\n";

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
}
