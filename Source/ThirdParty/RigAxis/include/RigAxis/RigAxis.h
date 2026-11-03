#ifndef RIGAXIS_H
#define RIGAXIS_H

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

#ifdef __cplusplus
}
#endif

#endif
