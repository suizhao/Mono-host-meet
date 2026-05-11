import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    width: 1280
    height: 840
    visible: true
    title: "MultiLive MVP"

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: HomePage { }
    }
}
