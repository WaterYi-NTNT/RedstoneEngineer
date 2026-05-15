#include "Camera.h"
#include <QtMath>

Camera::Camera()
{
    reset();
}

QQuaternion Camera::fromAzimuthElevation(float azDeg, float elDeg)
{
    QQuaternion yaw   = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0),  azDeg);
    QQuaternion pitch = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), -elDeg);
    return (yaw * pitch).normalized();
}

void Camera::reset()
{
    m_target      = QVector3D(8.0f, 2.0f, 8.0f);
    m_distance    = 28.0f;
    m_fov         = 45.0f;
    m_orientation = fromAzimuthElevation(45.0f, 30.0f);
}

void Camera::orbit(float dYaw, float dPitch)
{

    QQuaternion yawQ   = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), -dYaw);

    QVector3D   localX = m_orientation.rotatedVector(QVector3D(1, 0, 0));
    QQuaternion pitchQ = QQuaternion::fromAxisAndAngle(localX, -dPitch);

    m_orientation = (pitchQ * yawQ * m_orientation).normalized();
}

void Camera::pan(float dx, float dy)
{
    const float scale  = m_distance * 0.0015f;
    const QVector3D r  = rightVector();
    const QVector3D u  = upVector();
    m_target -= r * (dx * scale);
    m_target += u * (dy * scale);
}

void Camera::zoom(float delta)
{
    m_distance = qMax(1.5f, m_distance * qPow(0.9f, delta));
}

QVector3D Camera::rightVector() const
{
    return m_orientation.rotatedVector(QVector3D(1, 0, 0)).normalized();
}
QVector3D Camera::upVector() const
{
    return m_orientation.rotatedVector(QVector3D(0, 1, 0)).normalized();
}
QVector3D Camera::forwardVector() const
{

    return -m_orientation.rotatedVector(QVector3D(0, 0, 1)).normalized();
}

QVector3D Camera::position() const
{
    QVector3D offset = m_orientation.rotatedVector(QVector3D(0, 0, m_distance));
    return m_target + offset;
}

QMatrix4x4 Camera::viewMatrix() const
{
    QMatrix4x4 m;
    m.lookAt(position(), m_target, upVector());
    return m;
}

QMatrix4x4 Camera::projMatrix(float aspect) const
{
    QMatrix4x4 m;
    m.perspective(m_fov, aspect, 0.1f, 2000.0f);
    return m;
}
