import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    Component {
        id: groupDelegate
        Column {
            width: column.width
            spacing: Theme.paddingSmall
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: modelData.title
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                visible: String(modelData.stats).length > 0
                text: modelData.stats
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeTiny
            }
            Repeater {
                model: modelData.runs
                delegate: Label {
                    x: Theme.horizontalPageMargin
                    width: column.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("#%1  %2 ms  %3").arg(modelData.n).arg(modelData.ms).arg(modelData.mbps)
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Copy results")
                enabled: speed.ip.length > 0 || speed.pingText !== "…"
                onClicked: speed.copyResults()
            }
            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.animatorPush(Qt.resolvedUrl("AboutPage.qml"))
            }
        }

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Speedtest") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                text: speed.server.length ? speed.server : qsTr("speed.cloudflare.com")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Grid {
                id: hero
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                columns: 2
                rowSpacing: Theme.paddingLarge
                columnSpacing: Theme.paddingMedium

                Column {
                    width: (hero.width - hero.columnSpacing) / 2
                    Label { width: parent.width; text: speed.downText; color: Theme.highlightColor; font.pixelSize: Theme.fontSizeExtraLarge }
                    Label { width: parent.width; text: qsTr("Download"); color: Theme.secondaryHighlightColor; font.pixelSize: Theme.fontSizeTiny }
                    Label { width: parent.width; text: qsTr("90th percentile"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeTiny }
                }
                Column {
                    width: (hero.width - hero.columnSpacing) / 2
                    Label { width: parent.width; text: speed.upText; color: Theme.highlightColor; font.pixelSize: Theme.fontSizeExtraLarge }
                    Label { width: parent.width; text: qsTr("Upload"); color: Theme.secondaryHighlightColor; font.pixelSize: Theme.fontSizeTiny }
                    Label { width: parent.width; text: qsTr("90th percentile"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeTiny }
                }
                Column {
                    width: (hero.width - hero.columnSpacing) / 2
                    Label { width: parent.width; text: speed.pingText; color: Theme.highlightColor; font.pixelSize: Theme.fontSizeExtraLarge }
                    Label { width: parent.width; text: qsTr("Latency · ms"); color: Theme.secondaryHighlightColor; font.pixelSize: Theme.fontSizeTiny }
                    Label { width: parent.width; wrapMode: Text.Wrap; visible: speed.pingRange.length > 0; text: speed.pingRange; color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeTiny }
                }
                Column {
                    width: (hero.width - hero.columnSpacing) / 2
                    Label { width: parent.width; text: speed.jitterText; color: Theme.highlightColor; font.pixelSize: Theme.fontSizeExtraLarge }
                    Label { width: parent.width; text: qsTr("Jitter · ms"); color: Theme.secondaryHighlightColor; font.pixelSize: Theme.fontSizeTiny }
                    Label { width: parent.width; wrapMode: Text.Wrap; visible: speed.jitterRange.length > 0; text: speed.jitterRange; color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeTiny }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: speed.busy ? qsTr("Testing…") : qsTr("Start test")
                enabled: !speed.busy
                onClicked: speed.start()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                visible: speed.busy && speed.phase.length > 0
                text: speed.phase
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                visible: speed.error.length > 0
                text: speed.error
                color: "#ff6b6b"
            }

            SectionHeader { visible: speed.proto.length > 0; text: qsTr("Connection") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
                visible: speed.proto.length + speed.asn.length + speed.ip.length > 0
                text: [
                    speed.proto.length ? qsTr("Connected via %1").arg(speed.proto) : "",
                    speed.asn.length ? (speed.network.length
                                        ? qsTr("Network: %1 (AS%2)").arg(speed.network).arg(speed.asn)
                                        : qsTr("Network: AS%1").arg(speed.asn)) : "",
                    speed.ip.length ? qsTr("Your IP: %1").arg(speed.ip) : ""
                ].filter(function (s) { return s.length > 0 }).join("\n")
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                visible: speed.copyHint.length > 0
                text: speed.copyHint
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeTiny
            }

            SectionHeader { visible: speed.aimStreaming.length + speed.aimGaming.length + speed.aimRtc.length > 0; text: qsTr("Network quality") }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                visible: speed.aimStreaming.length + speed.aimGaming.length + speed.aimRtc.length > 0
                text: [
                    speed.aimStreaming.length ? qsTr("Streaming  %1").arg(speed.aimStreaming) : "",
                    speed.aimGaming.length ? qsTr("Gaming  %1").arg(speed.aimGaming) : "",
                    speed.aimRtc.length ? qsTr("Video calls  %1").arg(speed.aimRtc) : ""
                ].filter(function (s) { return s.length > 0 }).join("\n")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }

            SectionHeader { visible: speed.downGroups.length > 0; text: qsTr("Download") }
            Repeater { model: speed.downGroups; delegate: groupDelegate }
            SectionHeader { visible: speed.upGroups.length > 0; text: qsTr("Upload") }
            Repeater { model: speed.upGroups; delegate: groupDelegate }

            SectionHeader { visible: speed.idleLatencyStats.length > 0; text: qsTr("Latency") }
            Repeater {
                model: [speed.idleLatencyStats, speed.downLatencyStats, speed.upLatencyStats]
                delegate: Label {
                    x: Theme.horizontalPageMargin
                    width: column.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    visible: modelData.length > 0
                    text: modelData
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
            }
        }
        VerticalScrollDecorator {}
    }
}
