import QtQuick
import Lore.Avatar 1.0

Item {
    id: root

    property string modelSource: ""
    property real facing: 0.0
    property bool resizable: false

    readonly property int cornerGrip: 16
    readonly property int edgeGrip: 8

    readonly property real dragThreshold: 4.0

    function setViseme(shape) {
    }

    function playMotion(name) {
    }

    AvatarSurface {
        id: surface
        anchors.fill: parent
        objectName: "avatarSurface"

        cameraDistance: 0.45
        cameraYaw: 0.0
        cameraPitch: 0.0
        cameraFov: 45.0

        targetX: 0.05
        targetY: 1.56
        targetZ: 0.01

        modelScale: 1.0
        modelYaw: 0.0
        modelPitch: 0.0
        modelRoll: 0.0
    }

    // The interior input is inset by the edge band. A press on an
    // edge or a corner falls outside this item entirely, so the
    // QQuickWidget receives it and AvatarWidget::mousePressEvent
    // starts the move or resize. Anchoring to fill without the inset
    // means the MouseArea covers the edge band and the grips never
    // see a press.
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
        color: topEdgeHover.hovered ? "#ffffff28" : "transparent"

        HoverHandler {
            id: topEdgeHover
            cursorShape: Qt.SizeAllCursor
        }
    }

    Rectangle {
        visible: root.resizable
        z: 100
        x: root.cornerGrip
        y: root.height - root.edgeGrip
        width: root.width - 2 * root.cornerGrip
        height: root.edgeGrip
        color: bottomEdgeHover.hovered ? "#ffffff28" : "transparent"

        HoverHandler {
            id: bottomEdgeHover
            cursorShape: Qt.SizeAllCursor
        }
    }

    Rectangle {
        visible: root.resizable
        z: 100
        x: 0
        y: root.cornerGrip
        width: root.edgeGrip
        height: root.height - 2 * root.cornerGrip
        color: leftEdgeHover.hovered ? "#ffffff28" : "transparent"

        HoverHandler {
            id: leftEdgeHover
            cursorShape: Qt.SizeAllCursor
        }
    }

    Rectangle {
        visible: root.resizable
        z: 100
        x: root.width - root.edgeGrip
        y: root.cornerGrip
        width: root.edgeGrip
        height: root.height - 2 * root.cornerGrip
        color: rightEdgeHover.hovered ? "#ffffff28" : "transparent"

        HoverHandler {
            id: rightEdgeHover
            cursorShape: Qt.SizeAllCursor
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
            color: cornerHover.hovered ? "#ffffff20" : "transparent"

            HoverHandler {
                id: cornerHover
                cursorShape: (modelData.corner === 0 || modelData.corner === 3)
                    ? Qt.SizeFDiagCursor
                    : Qt.SizeBDiagCursor
            }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 4

        width: infoText.implicitWidth + 12
        height: infoText.implicitHeight + 8

        color: "#c0000000"
        radius: 4
        z: 200

        Text {
            id: infoText
            anchors.centerIn: parent

            color: "#ffffff"
            font.pixelSize: 11
            font.family: "monospace"

            text: "dist " + surface.cameraDistance.toFixed(2)
                + "  yaw " + surface.cameraYaw.toFixed(1)
                + "  pit " + surface.cameraPitch.toFixed(1)
                + "\ntgt " + surface.targetX.toFixed(2)
                + ", " + surface.targetY.toFixed(2)
                + ", " + surface.targetZ.toFixed(2)
        }
    }
}