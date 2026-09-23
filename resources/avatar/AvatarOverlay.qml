import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils

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

    // Emitted once the GLB has finished loading. AvatarWidget connects
    // to this and only calls attachToScene after it fires, because
    // RuntimeLoader is asynchronous and the joint nodes do not exist
    // in the scene graph until loading completes.
    signal modelLoaded()

    // Mirrors the loader's status so C++ can poll it as a fallback for
    // the case where the signal fired before the connection was made.
    // 0 = Null, 1 = Success, 2 = Loading, 3 = Error.
    property int modelLoaderStatus: modelLoader.status

    // Placeholders. Once the skeleton bridge is working, morph target
    // driving moves into AvatarController and these stop being called.
    function setViseme(shape) {
    }

    function playMotion(name) {
    }

    View3D {
        id: view
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
            eulerRotation.x: -25
            eulerRotation.y: -35
            color: Qt.rgba(1.0, 0.95, 0.88, 1.0)
            brightness: 1.1
            castsShadow: true
        }

        DirectionalLight {
            eulerRotation.x: 15
            eulerRotation.y: 55
            color: Qt.rgba(0.75, 0.82, 1.0, 1.0)
            brightness: 0.45
        }

        DirectionalLight {
            eulerRotation.x: -10
            eulerRotation.y: 160
            color: Qt.rgba(0.9, 0.9, 1.0, 1.0)
            brightness: 0.35
        }

        DirectionalLight {
            eulerRotation.x: 0
            eulerRotation.y: 0
            color: Qt.rgba(0.5, 0.5, 0.55, 1.0)
            brightness: 0.25
        }

        Node {
            id: figureAnchor
            objectName: "figureAnchor"
            y: root.figureOffsetY

            Node {
                id: modelRoot
                objectName: "modelRoot"
                scale: Qt.vector3d(root.modelScale, root.modelScale, root.modelScale)
                eulerRotation.y: root.facing

                RuntimeLoader {
                    id: modelLoader
                    source: "/home/sujan/Desktop/Lore/mutable/Lore.glb"

                    onStatusChanged: {
                        if (status === RuntimeLoader.Success) {
                            console.log("AvatarOverlay: model loaded")
                            root.modelLoaded()
                        } else if (status === RuntimeLoader.Error) {
                            console.warn("AvatarOverlay: model load failed:",
                                errorString)
                        }
                    }
                }
            }
        }
    }
}