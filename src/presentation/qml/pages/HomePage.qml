import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var homeController

    Rectangle {
        anchors.fill: parent
        color: "#16181d"
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12
        width: 480

        Label {
            text: "MultiLive - MVP 第一步"
            font.pixelSize: 26
            color: "white"
        }

        Label {
            text: homeController ? homeController.statusText : "controller not ready"
            color: "#b0b7c3"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        TextField {
            placeholderText: "昵称"
            text: homeController ? homeController.participantName : ""
            onTextChanged: {
                if (homeController) {
                    homeController.participantName = text
                }
            }
            Layout.fillWidth: true
        }

        TextField {
            placeholderText: "房间名"
            text: homeController ? homeController.roomName : ""
            onTextChanged: {
                if (homeController) {
                    homeController.roomName = text
                }
            }
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: "Demo 入会"
                Layout.fillWidth: true
                onClicked: {
                    if (homeController) {
                        homeController.startDemo()
                    }
                }
            }

            Button {
                text: "Custom 入会"
                Layout.fillWidth: true
                onClicked: {
                    if (homeController) {
                        homeController.startCustom("", "")
                    }
                }
            }
        }
    }
}
