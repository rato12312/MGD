#pragma once

#include "Plane.h"
#include "AABB.h"
#include "Mat4.h"
#include <cmath>

namespace mgd {

struct Frustum {
    Plane planes[6];

    enum { LEFT = 0, RIGHT = 1, BOTTOM = 2, TOP = 3, NEAR = 4, FAR = 5 };

    Frustum() = default;

    void extractFromVP(const Mat4& view, const Mat4& proj) {
        Mat4 vp = proj * view;

        Vec3 leftNormal(
            vp.m[3] + vp.m[0],
            vp.m[7] + vp.m[4],
            vp.m[11] + vp.m[8]
        );
        float leftD = vp.m[15] + vp.m[12];
        planes[LEFT] = Plane(leftNormal, leftD);

        Vec3 rightNormal(
            vp.m[3] - vp.m[0],
            vp.m[7] - vp.m[4],
            vp.m[11] - vp.m[8]
        );
        float rightD = vp.m[15] - vp.m[12];
        planes[RIGHT] = Plane(rightNormal, rightD);

        Vec3 bottomNormal(
            vp.m[3] + vp.m[1],
            vp.m[7] + vp.m[5],
            vp.m[11] + vp.m[9]
        );
        float bottomD = vp.m[15] + vp.m[13];
        planes[BOTTOM] = Plane(bottomNormal, bottomD);

        Vec3 topNormal(
            vp.m[3] - vp.m[1],
            vp.m[7] - vp.m[5],
            vp.m[11] - vp.m[9]
        );
        float topD = vp.m[15] - vp.m[13];
        planes[TOP] = Plane(topNormal, topD);

        // D3D clip convention: near at z = 0 -> plane is the 3rd row alone.
        Vec3 nearNormal(
            vp.m[2],
            vp.m[6],
            vp.m[10]
        );
        float nearD = vp.m[14];
        planes[NEAR] = Plane(nearNormal, nearD);

        Vec3 farNormal(
            vp.m[3] - vp.m[2],
            vp.m[7] - vp.m[6],
            vp.m[11] - vp.m[10]
        );
        float farD = vp.m[15] - vp.m[14];
        planes[FAR] = Plane(farNormal, farD);

        for (auto& p : planes) {
            p = p.normalized();
        }
    }

    bool containsPoint(const Vec3& p) const {
        for (int i = 0; i < 6; ++i) {
            if (planes[i].signedDistance(p) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    bool containsAABB(const AABB& box) const {
        for (int i = 0; i < 6; ++i) {
            Vec3 positiveVertex = box.min;
            if (planes[i].normal.x >= 0) positiveVertex.x = box.max.x;
            if (planes[i].normal.y >= 0) positiveVertex.y = box.max.y;
            if (planes[i].normal.z >= 0) positiveVertex.z = box.max.z;

            if (planes[i].signedDistance(positiveVertex) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    bool intersectsAABB(const AABB& box) const {
        for (int i = 0; i < 6; ++i) {
            Vec3 positiveVertex = box.min;
            if (planes[i].normal.x >= 0) positiveVertex.x = box.max.x;
            if (planes[i].normal.y >= 0) positiveVertex.y = box.max.y;
            if (planes[i].normal.z >= 0) positiveVertex.z = box.max.z;

            if (planes[i].signedDistance(positiveVertex) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    bool containsSphere(const Vec3& center, float radius) const {
        for (int i = 0; i < 6; ++i) {
            if (planes[i].signedDistance(center) < -radius) {
                return false;
            }
        }
        return true;
    }

    bool intersectsSphere(const Vec3& center, float radius) const {
        for (int i = 0; i < 6; ++i) {
            if (planes[i].signedDistance(center) < -radius) {
                return false;
            }
        }
        return true;
    }

    AABB getAABB() const {
        AABB result = AABB::invalid();
        Vec3 corners[8] = {
            {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
            {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
        };

        for (auto& c : corners) {
            result = result.expanded(c);
        }
        return result;
    }
};

} // namespace mgd
