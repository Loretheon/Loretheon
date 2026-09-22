import QtQuick
import QtQuick3D
import "qrc:/avatar/vita"

Item {
    id: root

    property string modelSource: ""

    // Framing and scale. Overridden at runtime by
    // AvatarWidget::applyConfig. The defaults here are the same as
    // AvatarConfig's, so the scene renders identically whether or not
    // applyConfig has been called.
    property real cameraDistance: 120.0
    property real cameraHeight: 45.0
    property real cameraPitch: -10.0
    property real fieldOfView: 45.0
    property real figureOffsetY: -20.0
    property real modelScale: 50.0

    property real mouthA: 0.0
    property real mouthE: 0.0
    property real mouthI: 0.0
    property real mouthO: 0.0
    property real mouthU: 0.0
    property real mouthM: 0.0

    property string expression: "neutral"

    function playMotion(name) {
        console.log("playMotion:", name)
    }

    function setViseme(shape) {
        mouthA = shape === "A" ? 1.0 : 0.0
        mouthE = shape === "E" ? 1.0 : 0.0
        mouthI = shape === "I" ? 1.0 : 0.0
        mouthO = shape === "O" ? 1.0 : 0.0
        mouthU = shape === "U" ? 1.0 : 0.0
        mouthM = (shape === "M" || shape === "sil") ? 1.0 : 0.0
    }

    View3D {
        anchors.fill: parent

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            position: Qt.vector3d(0, root.cameraHeight, root.cameraDistance)
            eulerRotation.x: root.cameraPitch
            fieldOfView: root.fieldOfView
        }

        DirectionalLight {
            eulerRotation.x: -30
            eulerRotation.y: -70
            brightness: 1.5
        }

        DirectionalLight {
            eulerRotation.x: 30
            eulerRotation.y: 140
            brightness: 0.5
        }

        Node {
            y: root.figureOffsetY

            Vita {
                id: vita
                scale: Qt.vector3d(root.modelScale, root.modelScale, root.modelScale)

                // Drive the face's morph targets from the viseme
                // properties on the root item. The face model inside
                // Vita.qml is the only model that carries mouth
                // morph targets; they are aliased on Vita's root as
                // faceMorphA/E/I/O/U.
                //
                // The VRM has no M (closed-mouth) morph target. When
                // the shape is "M" or "sil", all five targets are
                // driven to zero, which returns the face to its
                // neutral rest state — mouth closed. That is the
                // correct rest pose.
                Binding {
                    target: vita.faceMorphA
                    property: "weight"
                    value: root.mouthA
                }
                Binding {
                    target: vita.faceMorphE
                    property: "weight"
                    value: root.mouthE
                }
                Binding {
                    target: vita.faceMorphI
                    property: "weight"
                    value: root.mouthI
                }
                Binding {
                    target: vita.faceMorphO
                    property: "weight"
                    value: root.mouthO
                }
                Binding {
                    target: vita.faceMorphU
                    property: "weight"
                    value: root.mouthU
                }
            }
        }
    }
}