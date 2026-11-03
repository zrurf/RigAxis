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
typedef struct RigAxisRender RigAxisRender;

typedef struct RigAxisAffine
{
    double a;
    double b;
    double c;
    double d;
    double tx;
    double ty;
} RigAxisAffine;

typedef struct RigAxisVertex
{
    float x;
    float y;
    float u;
    float v;
    float light_r;
    float light_g;
    float light_b;
    float light_a;
    float dark_r;
    float dark_g;
    float dark_b;
    float dark_a;
} RigAxisVertex;

typedef struct RigAxisBatch
{
    uint32_t source;
    uint32_t page;
    uint32_t blend;
    uint32_t vertex_count;
    uint32_t index_count;
    const RigAxisVertex* vertices;
    const uint32_t* indices;
} RigAxisBatch;

typedef enum RigAxisEventKind
{
    RIGAXIS_EVENT_START = 0,
    RIGAXIS_EVENT_END = 1,
    RIGAXIS_EVENT_COMPLETE = 2,
    RIGAXIS_EVENT_KEY = 3
} RigAxisEventKind;

typedef struct RigAxisEvent
{
    uint32_t kind;
    uint32_t track;
    uint32_t animation;
    uint64_t entry;
    const char* animation_name;
    const char* event_name;
    double time;
    int64_t loop_index;
    int64_t int_value;
    double float_value;
    const char* string_value;
    const char* audio;
    double volume;
    double balance;
} RigAxisEvent;

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
uint32_t rigaxis_state_event_count(const RigAxisState* state);
bool rigaxis_state_event_at(const RigAxisState* state, uint32_t index, RigAxisEvent* out);
void rigaxis_state_clear_events(RigAxisState* state);

RigAxisStatus rigaxis_render_create(RigAxisRender** out);
void rigaxis_render_free(RigAxisRender* render);
RigAxisStatus rigaxis_render_build(RigAxisRender* render, const RigAxisSkeleton* skeleton, const RigAxisPose* pose, const RigAxisLookup* lookup, const float tint[4]);
uint32_t rigaxis_render_batch_count(const RigAxisRender* render);
bool rigaxis_render_batch_at(const RigAxisRender* render, uint32_t index, RigAxisBatch* out);

#ifdef __cplusplus
}
#endif

#endif
