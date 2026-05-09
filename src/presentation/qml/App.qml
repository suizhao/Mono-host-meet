import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: root
    width: 1000
    height: 680
    visible: true
    title: qsTr("MultiLive MVP")

    HomePage {
        anchors.fill: parent
        homeController: appContext.homeController
    }
}
