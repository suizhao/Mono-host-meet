import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "Home"
    property bool settingsPanelOpen: false
    property bool settingsPanelDidAutoRefresh: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "LiveKit Meet (QML MVP)"
            font.pixelSize: 24
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            visible: homeController.currentRoomName !== ""
            text: "房间号：#" + homeController.currentRoomName
            font.pixelSize: 20
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
            color: "#4CAF50"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#171a20"
            radius: 10
            border.width: 1
            border.color: "#3a3f4b"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Label {
                    text: roomController.remoteParticipantsText
                    color: "#d8dde8"
                    font.pixelSize: 18
                }

                Label {
                    text: roomController.remoteVideoSourceText
                    color: "#aeb8ca"
                    font.pixelSize: 14
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    property int tileCount: roomController.remoteVideoTileCount
                    columns: tileCount <= 1 ? 1 : (tileCount <= 4 ? 2 : 3)
                    columnSpacing: 10
                    rowSpacing: 10

                    Repeater {
                        model: roomController.remoteVideoTileModel
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#20242c"
                            radius: 8
                            border.width: 1
                            border.color: "#4a5260"

                            Image {
                                anchors.fill: parent
                                anchors.margins: 1
                                source: hasFrame ? imageUrl : ""
                                cache: false
                                fillMode: Image.PreserveAspectFit
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 28
                                color: "#66000000"

                                Label {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: displayName + " · " + sourceText
                                    color: "#e8edf8"
                                    font.pixelSize: 12
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: !hasFrame
                                text: "等待视频帧..."
                                color: "#909bb0"
                            }
                        }
                    }

                    Rectangle {
                        visible: roomController.remoteVideoTileCount === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#20242c"
                        radius: 8
                        border.width: 1
                        border.color: "#4a5260"

                        Label {
                            anchors.centerIn: parent
                            text: "暂无可渲染的视频轨道（等待摄像头或共享）"
                            color: "#c0c7d4"
                        }
                    }
                }

                // PiP：摄像头小窗浮动在屏幕共享右下角
                Rectangle {
                    visible: roomController.cameraPipVisible
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 12
                    width: 160
                    height: 120
                    color: "#20242c"
                    radius: 6
                    border.width: 1
                    border.color: "#4CAF50"
                    z: 10

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: roomController.cameraPipImageUrl
                        cache: false
                        fillMode: Image.PreserveAspectFit
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 20
                        color: "#66000000"

                        Label {
                            anchors.fill: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: "摄像头"
                            color: "#e8edf8"
                            font.pixelSize: 10
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: roomController.hideCameraPip()
                    }
                }
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Pane {
                    visible: homeController.prejoinVisible
                    Layout.fillWidth: true
                    padding: 8

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            text: "自定义入会"
                            font.bold: true
                            font.pixelSize: 16
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: "房间名:"
                                Layout.preferredWidth: 60
                            }

                            TextField {
                                id: roomNameInput
                                Layout.fillWidth: true
                                placeholderText: "输入房间号..."
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: "参与者:"
                                Layout.preferredWidth: 60
                            }

                            TextField {
                                id: participantNameInput
                                Layout.fillWidth: true
                                text: homeController.defaultParticipantName
                                placeholderText: "输入参与者名..."
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Button {
                                text: "加入"
                                Layout.fillWidth: true
                                onClicked: homeController.confirmCustomJoin(roomNameInput.text, participantNameInput.text)
                            }

                            Button {
                                text: "取消"
                                Layout.fillWidth: true
                                onClicked: homeController.cancelCustomJoin()
                            }
                        }
                    }
                }

                RowLayout {
                    visible: !homeController.prejoinVisible
                    Layout.fillWidth: true
                    spacing: 12

                    Button {
                        text: "Demo 开会"
                        enabled: !homeController.joinInProgress && !homeController.inRoom
                        Layout.fillWidth: true
                        onClicked: homeController.startDemoMeeting()
                    }

                    Button {
                        text: "Custom 连接"
                        enabled: !homeController.prejoinVisible && !homeController.joinInProgress && !homeController.inRoom
                        Layout.fillWidth: true
                        onClicked: homeController.openCustomMeeting()
                    }

                    Button {
                        text: settingsPanelOpen ? "收起设置面板" : "展开设置面板"
                        Layout.fillWidth: true
                        onClicked: {
                            const willOpen = !settingsPanelOpen
                            settingsPanelOpen = willOpen
                            if (willOpen && !settingsPanelDidAutoRefresh) {
                                roomController.refreshDevices()
                                settingsPanelDidAutoRefresh = true
                            }
                        }
                    }
                }

                RowLayout {
                    visible: !homeController.prejoinVisible
                    Layout.fillWidth: true
                    spacing: 12

                    Button {
                        text: roomController.micToggleText
                        enabled: homeController.inRoom
                        Layout.fillWidth: true
                        onClicked: roomController.toggleMic()
                    }

                    Button {
                        text: roomController.cameraToggleText
                        enabled: homeController.inRoom
                        Layout.fillWidth: true
                        onClicked: roomController.toggleCamera()
                    }

                    Button {
                        text: roomController.screenShareToggleText
                        enabled: homeController.inRoom
                        Layout.fillWidth: true
                        onClicked: roomController.toggleScreenShare()
                    }
                }

                Button {
                    visible: !homeController.prejoinVisible
                    text: "离开房间（SDK）"
                    enabled: homeController.inRoom
                    Layout.fillWidth: true
                    onClicked: homeController.leaveRoom()
                }

                Pane {
                    visible: settingsPanelOpen
                    Layout.fillWidth: true
                    padding: 10

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            text: "设备设置"
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Button {
                                text: "刷新设备列表"
                                Layout.preferredWidth: 160
                                onClicked: roomController.refreshDevices()
                            }

                            ComboBox {
                                id: audioInputBox
                                Layout.fillWidth: true
                                model: roomController.audioInputDevices
                                textRole: "name"
                                valueRole: "id"
                                onActivated: roomController.selectAudioInput(currentValue)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            ComboBox {
                                id: videoInputBox
                                Layout.fillWidth: true
                                model: roomController.videoInputDevices
                                textRole: "name"
                                valueRole: "id"
                                onActivated: roomController.selectVideoInput(currentValue)
                            }

                            ComboBox {
                                id: audioOutputBox
                                Layout.fillWidth: true
                                model: roomController.audioOutputDevices
                                textRole: "name"
                                valueRole: "id"
                                onActivated: roomController.selectAudioOutput(currentValue)
                            }
                        }

                        Label {
                            text: roomController.deviceStatusText
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#d0d0d0"
                        }

                        Label {
                            text: "录制设置"
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Button {
                                text: homeController.recordingActive ? "录制中" : "开始录制（API）"
                                enabled: homeController.inRoom && !homeController.recordingRequestInFlight
                                Layout.fillWidth: true
                                onClicked: homeController.startRecording()
                            }

                            Button {
                                text: "停止录制（API）"
                                enabled: homeController.inRoom && !homeController.recordingRequestInFlight
                                Layout.fillWidth: true
                                onClicked: homeController.stopRecording()
                            }
                        }

                        Label {
                            text: homeController.recordingStatusText
                        }
                    }
                }

                Label {
                    text: roomController.trackStatusText
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    visible: roomController.hasTrackError
                    text: roomController.trackErrorText
                    color: "red"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Label {
                    visible: homeController.hasErrorBanner
                    text: homeController.errorBannerText
                    color: "red"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Label {
                    text: homeController.statusMessage
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        Connections {
            target: homeController

            function onSessionResetRequested() {
                roomController.resetSessionUiState()
            }
        }
    }
}
