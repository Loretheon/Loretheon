import QtQuick
import Lore.Avatar 1.0

Item {
    id: root

    property string modelSource: ""
    property real facing: 0.0
    property bool resizable: false

    readonly property int cornerGrip: 18
    readonly property int edgeGrip: 10

    readonly property real dragThreshold: 4.0

    readonly property real referenceAspect: 1.0

    function setViseme(shape) {
    }

    function playMotion(name) {
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    AvatarSurface {
        id: surface

        objectName: "avatarSurface"

        readonly property real aspect: 1.0

        width: Math.min(parent.width, parent.height * aspect)
        height: width / aspect
        anchors.centerIn: parent

        cameraDistance: 2.42
        cameraYaw: 0.0
        cameraPitch: 0.0
        cameraFov: 45.0

        targetX: 0.0
        targetY: 0.853
        targetZ: 0.010

        modelScale: 1.0
        modelYaw: 0.0
        modelPitch: 0.0
        modelRoll: 0.0
    }

    MouseArea {
        id: interiorInput
        anchors.fill: parent
        anchors.margins: root.edgeGrip
        z: 0
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

        property real lastX: 0
        property real lastY: 0
        property real pressX: 0
        property real pressY: 0
        property bool dragging: false

        onPressed: (mouse) => {
            lastX = mouse.x
            lastY = mouse.y
            pressX = mouse.x
            pressY = mouse.y
            dragging = false
        }

        onPositionChanged: (mouse) => {
            if (!pressed) return

            if (!dragging) {
                const totalX = Math.abs(mouse.x - pressX)
                const totalY = Math.abs(mouse.y - pressY)

                if (totalX < root.dragThreshold &&
                    totalY < root.dragThreshold) {
                    return
                }

                dragging = true
                lastX = pressX
                lastY = pressY
            }

            const dx = mouse.x - lastX
            const dy = mouse.y - lastY
            lastX = mouse.x
            lastY = mouse.y

            if (mouse.buttons & (Qt.RightButton | Qt.MiddleButton)) {
                surface.panBy(dx, dy)
            } else if (mouse.buttons & Qt.LeftButton) {
                surface.orbitBy(dx, dy)
            }
        }

        onReleased: {
            dragging = false
        }

        onCanceled: {
            dragging = false
        }

        onDoubleClicked: {
            surface.resetCamera()
        }
    }

    WheelHandler {
        onWheel: (event) => {
            surface.zoomBy(event.angleDelta.y / 120.0)
        }
    }

    Rectangle {
        visible: root.resizable
        z: 100
        x: root.cornerGrip
        y: 0
        width: root.width - 2 * root.cornerGrip
        height: root.edgeGrip
        color: topEdgeHover.hovered ? "#3a7bd544" : "transparent"

        HoverHandler {
            id: topEdgeHover
            cursorShape: Qt.SizeAllCursor
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    Rectangle {
        visible: root.resizable
        z: 100
        x: root.cornerGrip
        y: root.height - root.edgeGrip
        width: root.width - 2 * root.cornerGrip
        height: root.edgeGrip
        color: bottomEdgeHover.hovered ? "#3a7bd544" : "transparent"

        HoverHandler {
            id: bottomEdgeHover
            cursorShape: Qt.SizeAllCursor
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    Rectangle {
        visible: root.resizable
        z: 100
        x: 0
        y: root.cornerGrip
        width: root.edgeGrip
        height: root.height - 2 * root.cornerGrip
        color: leftEdgeHover.hovered ? "#3a7bd544" : "transparent"

        HoverHandler {
            id: leftEdgeHover
            cursorShape: Qt.SizeAllCursor
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    Rectangle {
        visible: root.resizable
        z: 100
        x: root.width - root.edgeGrip
        y: root.cornerGrip
        width: root.edgeGrip
        height: root.height - 2 * root.cornerGrip
        color: rightEdgeHover.hovered ? "#3a7bd544" : "transparent"

        HoverHandler {
            id: rightEdgeHover
            cursorShape: Qt.SizeAllCursor
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    Repeater {
        model: [
            { corner: 0 },
            { corner: 1 },
            { corner: 2 },
            { corner: 3 }
        ]

        delegate: Rectangle {
            visible: root.resizable
            z: 110
            x: modelData.corner === 0 || modelData.corner === 2
                ? 0
                : root.width - root.cornerGrip
            y: modelData.corner === 0 || modelData.corner === 1
                ? 0
                : root.height - root.cornerGrip
            width: root.cornerGrip
            height: root.cornerGrip
            color: cornerHover.hovered ? "#3a7bd566" : "transparent"

            HoverHandler {
                id: cornerHover
                cursorShape: (modelData.corner === 0 || modelData.corner === 3)
                    ? Qt.SizeFDiagCursor
                    : Qt.SizeBDiagCursor
            }

            Behavior on color {
                ColorAnimation { duration: 120 }
            }
        }
    }
}