import QtQuick
import QtQuick3D
import "qrc:/avatar/vita"

Item {
    id: root

    property string modelSource: ""

    property real cameraDistance: 120.0
    property real cameraHeight: 45.0
    property real cameraPitch: -10.0
    property real fieldOfView: 45.0
    property real figureOffsetY: -20.0
    property real modelScale: 50.0

    property real facing: 0.0

    // Mouth shape weights. These are the targets. The Behavior blocks
    // below animate toward the target rather than snapping, which is
    // what makes the mouth co-articulate instead of stepping.
    property real mouthA: 0.0
    property real mouthE: 0.0
    property real mouthI: 0.0
    property real mouthO: 0.0
    property real mouthU: 0.0
    property real mouthM: 0.0

    Behavior on mouthA { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
    Behavior on mouthE { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
    Behavior on mouthI { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
    Behavior on mouthO { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
    Behavior on mouthU { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
    Behavior on mouthM { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }

    Behavior on facing { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    property string expression: "neutral"

    function playMotion(name) {
        console.log("playMotion:", name)
    }

    // Collapse one of the Oculus 15 viseme codes to the five VRM
    // mouth morph targets.
    //
    // Vowel codes drive their channel at full weight. Consonant codes
    // drive a combination of closed-mouth (M) and a small vowel
    // channel, because the visible mouth shape for a consonant is
    // usually a partly-closed mouth with the lips in a specific
    // posture. The values here are a first pass; they are the numbers
    // to tune if a particular phoneme looks wrong.
    //
    // RR is a tongue consonant, not a vowel. It drives a mostly
    // closed mouth with no rounding. Driving it with O was making
    // every r and l in English read as a vowel.
    function setViseme(shape) {
        var a = 0.0
        var e = 0.0
        var i = 0.0
        var o = 0.0
        var u = 0.0
        var m = 0.0

        switch (shape) {
            case "aa":
                a = 1.0
                break
            case "E":
                e = 1.0
                break
            case "I":
                i = 1.0
                break
            case "O":
                o = 1.0
                break
            case "U":
                u = 1.0
                break
            case "PP":
                m = 0.85
                break
            case "FF":
                m = 0.35
                i = 0.15
                break
            case "TH":
                m = 0.25
                i = 0.25
                break
            case "DD":
                m = 0.30
                e = 0.20
                break
            case "kk":
                m = 0.25
                e = 0.20
                break
            case "CH":
                m = 0.35
                i = 0.25
                break
            case "SS":
                m = 0.25
                i = 0.35
                break
            case "nn":
                m = 0.35
                e = 0.15
                break
            case "RR":
                m = 0.55
                break
            case "sil":
            default:
                m = 1.0
                break
        }

        mouthA = a
        mouthE = e
        mouthI = i
        mouthO = o
        mouthU = u
        mouthM = m
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

        // Key light: warm, from her front-left and above.
        DirectionalLight {
            eulerRotation.x: -25
            eulerRotation.y: -35
            color: Qt.rgba(1.0, 0.95, 0.88, 1.0)
            brightness: 1.1
            castsShadow: true
        }

        // Fill: cool, from her front-right, dimmer.
        DirectionalLight {
            eulerRotation.x: 15
            eulerRotation.y: 55
            color: Qt.rgba(0.75, 0.82, 1.0, 1.0)
            brightness: 0.45
        }

        // Rim: from behind, to separate her from the background.
        DirectionalLight {
            eulerRotation.x: -10
            eulerRotation.y: 160
            color: Qt.rgba(0.9, 0.9, 1.0, 1.0)
            brightness: 0.35
        }

        // Ambient substitute: straight-on, very dim, colour-neutral.
        DirectionalLight {
            eulerRotation.x: 0
            eulerRotation.y: 0
            color: Qt.rgba(0.5, 0.5, 0.55, 1.0)
            brightness: 0.25
        }

        Node {
            y: root.figureOffsetY

            Vita {
                id: vita
                scale: Qt.vector3d(root.modelScale, root.modelScale, root.modelScale)
                eulerRotation.y: root.facing

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