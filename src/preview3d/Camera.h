#pragma once

#include <QVector3D>
#include <QMatrix4x4>
#include <QQuaternion>


class Camera
{
public:
    Camera();


    void orbit (float dYaw, float dPitch);
    void pan   (float dx,   float dy);
    void zoom  (float delta);
    void reset ();


    QMatrix4x4 viewMatrix()         const;
    QMatrix4x4 projMatrix(float aspect) const;
    QVector3D  position()           const;
    QVector3D  upVector()           const;
    QVector3D  rightVector()        const;
    QVector3D  forwardVector()      const;


    void setTarget  (const QVector3D &t) { m_target   = t; }
    void setDistance(float d)            { m_distance = qMax(1.0f, d); }
    void setFov     (float f)            { m_fov      = qBound(10.f, f, 120.f); }

    QVector3D   target()      const { return m_target;      }
    float       distance()    const { return m_distance;    }
    float       fov()         const { return m_fov;         }
    QQuaternion orientation() const { return m_orientation; }

private:
    QVector3D   m_target;
    float       m_distance;
    float       m_fov;
    QQuaternion m_orientation;


    static QQuaternion fromAzimuthElevation(float azDeg, float elDeg);
};
