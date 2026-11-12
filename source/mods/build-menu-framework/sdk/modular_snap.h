#ifndef MOONLAB_MODULAR_SNAP_H
#define MOONLAB_MODULAR_SNAP_H
#include "construction.h"

/* Candidate geometry only. Native plans/checks still own support, collision,
 * unlocks and payment. Frames and sockets must come from measured native or
 * authored data; this helper neither guesses module sizes nor rotates ghosts. */
typedef struct {
    MoonBuildPoint origin, right, up, forward;
} MoonBuildFrame;
typedef struct {
    unsigned int id, channel;
    MoonBuildPoint point, normal;
} MoonBuildSocket;
typedef struct {
    MoonBuildPoint origin;
    float distance2;
    unsigned long long owner;
    unsigned int target_socket, source_socket;
    int found;
} MoonBuildSnapChoice;
typedef struct {
    MoonBuildPoint scale, translation;
} MoonBuildModuleFit;

static inline int moon_build_finite(float v) {
    return v == v && v >= -3.402823466e38f && v <= 3.402823466e38f;
}
static inline int moon_build_point_finite(MoonBuildPoint p) {
    return moon_build_finite(p.x) && moon_build_finite(p.y) && moon_build_finite(p.z);
}
static inline float moon_build_dot(MoonBuildPoint a, MoonBuildPoint b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline int moon_build_near(float v, float wanted) {
    return moon_build_finite(v) && v >= wanted - .001f && v <= wanted + .001f;
}
static inline int moon_build_unit(MoonBuildPoint p) {
    return moon_build_point_finite(p) && moon_build_near(moon_build_dot(p, p), 1);
}
static inline int moon_build_frame_valid(const MoonBuildFrame *frame) {
    if (!frame || !moon_build_point_finite(frame->origin) || !moon_build_unit(frame->right) ||
        !moon_build_unit(frame->up) || !moon_build_unit(frame->forward) ||
        !moon_build_near(moon_build_dot(frame->right, frame->up), 0) ||
        !moon_build_near(moon_build_dot(frame->right, frame->forward), 0) ||
        !moon_build_near(moon_build_dot(frame->up, frame->forward), 0))
        return 0;
    MoonBuildPoint cross = {
        frame->right.y * frame->up.z - frame->right.z * frame->up.y,
        frame->right.z * frame->up.x - frame->right.x * frame->up.z,
        frame->right.x * frame->up.y - frame->right.y * frame->up.x};
    return moon_build_near(moon_build_dot(cross, frame->forward), 1);
}
static inline MoonBuildPoint moon_build_frame_vector(const MoonBuildFrame *f, MoonBuildPoint p) {
    return (MoonBuildPoint){f->right.x * p.x + f->up.x * p.y + f->forward.x * p.z,
                            f->right.y * p.x + f->up.y * p.y + f->forward.y * p.z,
                            f->right.z * p.x + f->up.z * p.y + f->forward.z * p.z};
}
static inline MoonBuildPoint moon_build_frame_point(const MoonBuildFrame *f, MoonBuildPoint p) {
    MoonBuildPoint q = moon_build_frame_vector(f, p);
    return (MoonBuildPoint){q.x + f->origin.x, q.y + f->origin.y, q.z + f->origin.z};
}
/* Reset once per preview query. Exact-distance ties use stable native owner
 * and socket IDs, so enumeration order cannot make the ghost jump. */
static inline void moon_build_snap_reset(MoonBuildSnapChoice *choice) {
    if (choice)
        *choice = (MoonBuildSnapChoice){0};
}
static inline int moon_build_snap_consider(MoonBuildSnapChoice *choice,
                                           const MoonBuildFrame *moving, const MoonBuildSocket *source,
                                           const MoonBuildFrame *fixed, const MoonBuildSocket *target,
                                           unsigned long long owner, float reach) {
    if (!choice || !source || !target || !moon_build_frame_valid(moving) ||
        !moon_build_frame_valid(fixed) || !moon_build_finite(reach) || reach < 0 ||
        !source->channel || source->channel != target->channel ||
        !moon_build_point_finite(source->point) || !moon_build_point_finite(target->point) ||
        !moon_build_unit(source->normal) || !moon_build_unit(target->normal))
        return 0;
    MoonBuildPoint sn = moon_build_frame_vector(moving, source->normal),
                   tn = moon_build_frame_vector(fixed, target->normal);
    if (!moon_build_near(moon_build_dot(sn, tn), -1))
        return 0;
    MoonBuildPoint a = moon_build_frame_point(moving, source->point),
                   b = moon_build_frame_point(fixed, target->point);
    MoonBuildPoint origin = {moving->origin.x + b.x - a.x, moving->origin.y + b.y - a.y,
                              moving->origin.z + b.z - a.z};
    float distance2 = moon_build_distance2(a, b), limit2 = reach * reach;
    if (!moon_build_point_finite(origin) || !moon_build_finite(distance2) ||
        !moon_build_finite(limit2) || distance2 > limit2)
        return 0;
    if (choice->found && (distance2 > choice->distance2 ||
        (distance2 == choice->distance2 && (owner > choice->owner ||
        (owner == choice->owner && (target->id > choice->target_socket ||
        (target->id == choice->target_socket && source->id >= choice->source_socket)))))))
        return 0;
    *choice = (MoonBuildSnapChoice){origin, distance2, owner, target->id, source->id, 1};
    return 1;
}

/* Authoring proposal: fit measured bounds to a measured construction envelope.
 * No mesh, collider, native definition or saved transform is changed here.
 * Translation preserves the requested datum even for off-centre wall pivots. */
static inline int moon_build_module_fit(MoonBuildPoint authored_min, MoonBuildPoint authored_max,
                                        MoonBuildPoint target_min, MoonBuildPoint target_max,
                                        float max_relative_change, MoonBuildModuleFit *out) {
    if (!out || !moon_build_point_finite(authored_min) || !moon_build_point_finite(authored_max) ||
        !moon_build_point_finite(target_min) || !moon_build_point_finite(target_max) ||
        !moon_build_finite(max_relative_change) || max_relative_change < 0)
        return 0;
    float lo[] = {authored_min.x, authored_min.y, authored_min.z},
          hi[] = {authored_max.x, authored_max.y, authored_max.z},
          wanted_lo[] = {target_min.x, target_min.y, target_min.z},
          wanted_hi[] = {target_max.x, target_max.y, target_max.z}, scale[3], offset[3];
    for (int i = 0; i < 3; ++i) {
        float span = hi[i] - lo[i], wanted = wanted_hi[i] - wanted_lo[i];
        if (!moon_build_finite(span) || !moon_build_finite(wanted) || span <= 0 || wanted <= 0)
            return 0;
        scale[i] = wanted / span;
        offset[i] = wanted_lo[i] - lo[i] * scale[i];
        if (!moon_build_finite(scale[i]) || scale[i] <= 0 || !moon_build_finite(offset[i]) ||
            scale[i] < 1 - max_relative_change || scale[i] > 1 + max_relative_change)
            return 0;
    }
    *out = (MoonBuildModuleFit){{scale[0], scale[1], scale[2]}, {offset[0], offset[1], offset[2]}};
    return 1;
}
#endif
