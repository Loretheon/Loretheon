import QtQuick
import QtQuick3D
import "qrc:/avatar/vita"

Item {
    id: root

    property string modelSource: ""

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
            position: Qt.vector3d(0, 45, 120)
            eulerRotation.x: -10
            fieldOfView: 45
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
            y: -20

            Vita {
                scale: Qt.vector3d(50, 50, 50)
            }
        }
    }
}