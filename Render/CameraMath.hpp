#pragma once
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NF::Render {
    
    struct Vec3 {
        float x, y, z;
        Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
        Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
        Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
        Vec3 normalize() const {
            float len = std::sqrt(x*x + y*y + z*z);
            if(len == 0) return {0,0,0};
            return {x/len, y/len, z/len};
        }
        Vec3 cross(const Vec3& o) const {
            return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
        }
        float dot(const Vec3& o) const {
            return x*o.x + y*o.y + z*o.z;
        }
    };

    struct Mat4 {
        float m[4][4] = {{0}};

        static Mat4 identity() {
            Mat4 res;
            res.m[0][0] = 1; res.m[1][1] = 1; res.m[2][2] = 1; res.m[3][3] = 1;
            return res;
        }

        Mat4 operator*(const Mat4& o) const {
            Mat4 res;
            for(int c=0; c<4; ++c)
                for(int r=0; r<4; ++r)
                    res.m[c][r] = m[0][r]*o.m[c][0] + m[1][r]*o.m[c][1] + m[2][r]*o.m[c][2] + m[3][r]*o.m[c][3];
            return res;
        }

        static Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
            Vec3 f = (center - eye).normalize();
            Vec3 s = f.cross(up).normalize();
            Vec3 u = s.cross(f);
            Mat4 res = identity();
            res.m[0][0] = s.x; res.m[1][0] = s.y; res.m[2][0] = s.z;
            res.m[0][1] = u.x; res.m[1][1] = u.y; res.m[2][1] = u.z;
            res.m[0][2] =-f.x; res.m[1][2] =-f.y; res.m[2][2] =-f.z;
            res.m[3][0] = -s.dot(eye);
            res.m[3][1] = -u.dot(eye);
            res.m[3][2] = f.dot(eye);
            return res;
        }

        static Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
            float tanHalfFovy = std::tan(fovY / 2.0f);
            Mat4 res;
            res.m[0][0] = 1.0f / (aspect * tanHalfFovy);
            res.m[1][1] = -1.0f / (tanHalfFovy);
            res.m[2][2] = zFar / (zNear - zFar);
            res.m[2][3] = -1.0f;
            res.m[3][2] = -(zFar * zNear) / (zFar - zNear);
            return res;
        }
    };

    // --- ÚJ: FRUSTUM CULLING MATEMATIKA ---
    struct Plane {
        float x, y, z, d;
        void normalize() {
            float mag = std::sqrt(x*x + y*y + z*z);
            x /= mag; y /= mag; z /= mag; d /= mag;
        }
    };

    struct Frustum {
        Plane planes[6];

        static Frustum extract(const Mat4& vp) {
            Frustum f;
            f.planes[0] = {vp.m[0][3] + vp.m[0][0], vp.m[1][3] + vp.m[1][0], vp.m[2][3] + vp.m[2][0], vp.m[3][3] + vp.m[3][0]}; // Bal
            f.planes[1] = {vp.m[0][3] - vp.m[0][0], vp.m[1][3] - vp.m[1][0], vp.m[2][3] - vp.m[2][0], vp.m[3][3] - vp.m[3][0]}; // Jobb
            f.planes[2] = {vp.m[0][3] + vp.m[0][1], vp.m[1][3] + vp.m[1][1], vp.m[2][3] + vp.m[2][1], vp.m[3][3] + vp.m[3][1]}; // Alsó
            f.planes[3] = {vp.m[0][3] - vp.m[0][1], vp.m[1][3] - vp.m[1][1], vp.m[2][3] - vp.m[2][1], vp.m[3][3] - vp.m[3][1]}; // Felső
            f.planes[4] = {vp.m[0][2], vp.m[1][2], vp.m[2][2], vp.m[3][2]}; // Közeli (Near)
            f.planes[5] = {vp.m[0][3] - vp.m[0][2], vp.m[1][3] - vp.m[1][2], vp.m[2][3] - vp.m[2][2], vp.m[3][3] - vp.m[3][2]}; // Távoli (Far)

            for(int i=0; i<6; ++i) f.planes[i].normalize();
            return f;
        }

        bool isBoxVisible(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
            for(int i = 0; i < 6; ++i) {
                float px = (planes[i].x > 0) ? maxX : minX;
                float py = (planes[i].y > 0) ? maxY : minY;
                float pz = (planes[i].z > 0) ? maxZ : minZ;
                if (planes[i].x * px + planes[i].y * py + planes[i].z * pz + planes[i].d < 0) {
                    return false; // A doboz KÍVÜL esik ezen a síkon! Láthatatlan.
                }
            }
            return true; // Ha egyetlen sík se vágta le, akkor látható!
        }
    };
}