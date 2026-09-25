import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingMedium
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Speedtest")
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeLarge
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: speed.busy ? speed.phase : qsTr("%1 ms").arg(speed.pingText)
            color: Theme.primaryColor
            font.pixelSize: Theme.fontSizeMedium
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("%1 ↓  %2 ↑").arg(speed.downText).arg(speed.upText)
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-cover-refresh"
            onTriggered: if (!speed.busy) speed.start()
        }
    }
}
