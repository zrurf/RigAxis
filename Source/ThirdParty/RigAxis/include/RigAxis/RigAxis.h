#ifndef RIGAXIS_H
#define RIGAXIS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIGAXIS_ABI_VERSION 1

typedef enum RigAxisStatus
{
    RIGAXIS_OK = 0,
    RIGAXIS_ERROR_INVALID_ARGUMENT = 1,
    RIGAXIS_ERROR_INVALID_DATA = 2,
    RIGAXIS_ERROR_OUT_OF_MEMORY = 3,
    RIGAXIS_ERROR_UNSUPPORTED = 4,
    RIGAXIS_ERROR_INTERNAL = 5
} RigAxisStatus;

typedef struct RigAxisSkeleton RigAxisSkeleton;
typedef struct RigAxisAtlas RigAxisAtlas;
typedef struct RigAxisLookup RigAxisLookup;
typedef struct RigAxisPose RigAxisPose;
typedef struct RigAxisState RigAxisState;

typedef struct RigAxisAffine
{
    double a;
    double b;
    double c;
    double d;
    double tx;
    double ty;
} RigAxisAffine;

uint32_t rigaxis_abi_version(void);
const char* rigaxis_version_string(void);
const char* rigaxis_last_error(void);

RigAxisStatus rigaxis_skeleton_create(const uint8_t* data, size_t size, RigAxisSkeleton** out);
void rigaxis_skeleton_free(RigAxisSkeleton* skeleton);
const char* rigaxis_skeleton_format(const RigAxisSkeleton* skeleton);
uint32_t rigaxis_skeleton_bone_count(const RigAxisSkeleton* skeleton);
const char* rigaxis_skeleton_bone_name(const RigAxisSkeleton* skeleton, uint32_t index);
int32_t rigaxis_skeleton_find_bone(const RigAxisSkeleton* skeleton, const char* name);
uint32_t rigaxis_skeleton_animation_count(const RigAxisSkeleton* skeleton);
const char* rigaxis_skeleton_animation_name(const RigAxisSkeleton* skeleton, uint32_t index);
int32_t rigaxis_skeleton_find_animation(const RigAxisSkeleton* skeleton, const char* name);
uint32_t rigaxis_skeleton_skin_count(const RigAxisSkeleton* skeleton);
const char* rigaxis_skeleton_skin_name(const RigAxisSkeleton* skeleton, uint32_t index);
uint32_t rigaxis_skeleton_warning_count(const RigAxisSkeleton* skeleton);
const char* rigaxis_skeleton_warning_at(const RigAxisSkeleton* skeleton, uint32_t index);

RigAxisStatus rigaxis_atlas_create(const uint8_t* text, size_t size, RigAxisAtlas** out);
void rigaxis_atlas_free(RigAxisAtlas* atlas);
uint32_t rigaxis_atlas_page_count(const RigAxisAtlas* atlas);
const char* rigaxis_atlas_page_name(const RigAxisAtlas* atlas, uint32_t index);

RigAxisStatus rigaxis_lookup_create(const RigAxisSkeleton* skeleton, const RigAxisAtlas* atlas, RigAxisLookup** out);
void rigaxis_lookup_free(RigAxisLookup* lookup);
uint32_t rigaxis_lookup_missing_count(const RigAxisLookup* lookup);
const char* rigaxis_lookup_missing_at(const RigAxisLookup* lookup, uint32_t index);

RigAxisStatus rigaxis_pose_create(const RigAxisSkeleton* skeleton, RigAxisPose** out);
void rigaxis_pose_free(RigAxisPose* pose);
void rigaxis_pose_reset(const RigAxisSkeleton* skeleton, RigAxisPose* pose);
void rigaxis_pose_update_world(const RigAxisSkeleton* skeleton, RigAxisPose* pose);
void rigaxis_pose_reset_physics(RigAxisPose* pose);
bool rigaxis_pose_set_skin(const RigAxisSkeleton* skeleton, RigAxisPose* pose, const char* name);
bool rigaxis_pose_bone_world(const RigAxisPose* pose, uint32_t bone, RigAxisAffine* out);

RigAxisStatus rigaxis_state_create(RigAxisState** out);
void rigaxis_state_free(RigAxisState* state);
void rigaxis_state_set_speed(RigAxisState* state, double speed);
double rigaxis_state_get_speed(const RigAxisState* state);
bool rigaxis_state_play(RigAxisState* state, const RigAxisSkeleton* skeleton, const char* name, bool looping, double mix, uint64_t* out_entry);
bool rigaxis_state_set_animation(RigAxisState* state, const RigAxisSkeleton* skeleton, uint32_t track, const char* name, bool looping, double mix, uint64_t* out_entry);
bool rigaxis_state_add_animation(RigAxisState* state, const RigAxisSkeleton* skeleton, uint32_t track, const char* name, bool looping, double mix, double delay, bool has_delay, uint64_t* out_entry);
void rigaxis_state_clear_track(RigAxisState* state, uint32_t track);
void rigaxis_state_stop(RigAxisState* state);
bool rigaxis_state_set_track_alpha(RigAxisState* state, uint32_t track, double alpha);
bool rigaxis_state_set_track_time_scale(RigAxisState* state, uint32_t track, double scale);
bool rigaxis_state_set_track_time(RigAxisState* state, uint32_t track, double time);
void rigaxis_state_update(RigAxisState* state, const RigAxisSkeleton* skeleton, double delta_seconds);
void rigaxis_state_apply(RigAxisState* state, const RigAxisSkeleton* skeleton, RigAxisPose* pose);

#ifdef __cplusplus
}
#endif

#endif
