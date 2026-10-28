import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaking

import "controls" as C

// One-click stream presets. Each preset writes the same handful of settings
// that the full Settings dialog exposes (resolution, fps, bitrate, codec and
// the renderer quality preset) for both PS4 and PS5, local and remote.
DialogView {
    id: dialog
    title: qsTr("Quick Setup")
    buttonVisible: false

    // resolution: 1=360p 2=540p 3=720p 4=1080p, bitrate in kbps (0 = automatic)
    // codec: 0=H.264 1=H.265 2=H.265 HDR, renderer: 0=Fast 1=Default 2=High Quality
    readonly property var presets: [
        {
            key: "competitive",
            name: qsTr("Competitive"),
            summary: qsTr("Lowest input lag"),
            details: qsTr("1080p · 60 fps · 30 Mbps at home\n720p · 60 fps · 10 Mbps away\nFast renderer (no extra post-processing)"),
            localRes: 4, remoteRes: 3, localBitrate: 30000, remoteBitrate: 10000,
            codec: 1, renderer: 0, windowPreset: ChiakiWindow.VideoPreset.Fast
        },
        {
            key: "balanced",
            name: qsTr("Balanced"),
            summary: qsTr("Recommended for most setups"),
            details: qsTr("1080p · 60 fps · automatic bitrate at home\n720p · 60 fps · automatic bitrate away\nDefault renderer"),
            localRes: 4, remoteRes: 3, localBitrate: 0, remoteBitrate: 0,
            codec: 1, renderer: 1, windowPreset: ChiakiWindow.VideoPreset.Default
        },
        {
            key: "quality",
            name: qsTr("Quality"),
            summary: qsTr("Sharpest picture, a little more lag"),
            details: qsTr("1080p · 60 fps · 50 Mbps at home\n1080p · 60 fps · 20 Mbps away\nHigh-quality upscaling"),
            localRes: 4, remoteRes: 4, localBitrate: 50000, remoteBitrate: 20000,
            codec: 1, renderer: 2, windowPreset: ChiakiWindow.VideoPreset.HighQuality
        }
    ]

    property string appliedName: ""

    function matches(p) {
        let s = Chiaki.settings;
        return s.resolutionLocalPS5 === p.localRes && s.resolutionRemotePS5 === p.remoteRes
            && s.fpsLocalPS5 === 60 && s.fpsRemotePS5 === 60
            && s.bitrateLocalPS5 === p.localBitrate && s.bitrateRemotePS5 === p.remoteBitrate
            && s.codecLocalPS5 === p.codec && s.codecRemotePS5 === p.codec
            && s.videoPreset === p.renderer;
    }

    function apply(p) {
        let s = Chiaki.settings;
        // PS5
        s.resolutionLocalPS5 = p.localRes;
        s.resolutionRemotePS5 = p.remoteRes;
        s.fpsLocalPS5 = 60;
        s.fpsRemotePS5 = 60;
        s.bitrateLocalPS5 = p.localBitrate;
        s.bitrateRemotePS5 = p.remoteBitrate;
        s.codecLocalPS5 = p.codec;
        s.codecRemotePS5 = p.codec;
        // PS4 (no codec choice, base PS4 tops out lower so bitrate stays automatic)
        s.resolutionLocalPS4 = p.localRes;
        s.resolutionRemotePS4 = p.remoteRes;
        s.fpsLocalPS4 = 60;
        s.fpsRemotePS4 = 60;
        s.bitrateLocalPS4 = 0;
        s.bitrateRemotePS4 = 0;
        // Renderer
        s.videoPreset = p.renderer;
        Chiaki.window.videoPreset = p.windowPreset;
        appliedName = p.name;
    }

    Item {
        ColumnLayout {
            anchors {
                top: parent.top
                horizontalCenter: parent.horizontalCenter
                topMargin: 30
            }
            spacing: 24

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Pick how you want to play. You can still fine-tune everything in Settings.")
                font.pixelSize: 18
                opacity: 0.8
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 24

                Repeater {
                    model: dialog.presets

                    C.Button {
                        id: card
                        required property var modelData
                        required property int index
                        readonly property bool active: dialog.appliedName === modelData.name
                                                       || (dialog.appliedName === "" && dialog.matches(modelData))
                        firstInFocusChain: index === 0
                        lastInFocusChain: index === dialog.presets.length - 1
                        Layout.preferredWidth: 340
                        Layout.preferredHeight: 260
                        Material.roundedScale: Material.MediumScale
                        highlighted: active
                        onClicked: dialog.apply(modelData)
                        Keys.onLeftPressed: { let i = nextItemInFocusChain(false); if (i) i.forceActiveFocus(Qt.TabFocusReason); }
                        Keys.onRightPressed: { let i = nextItemInFocusChain(); if (i) i.forceActiveFocus(Qt.TabFocusReason); }

                        contentItem: ColumnLayout {
                            spacing: 10
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: card.modelData.name + (card.active ? "  ✓" : "")
                                font.pixelSize: 28
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: card.modelData.summary
                                font.pixelSize: 16
                                color: Material.accent
                            }
                            Label {
                                Layout.fillWidth: true
                                Layout.topMargin: 8
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: card.modelData.details
                                font.pixelSize: 14
                                opacity: 0.8
                            }
                        }
                    }
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                visible: dialog.appliedName !== ""
                text: qsTr("%1 preset applied. It takes effect on your next connection.").arg(dialog.appliedName)
                font.pixelSize: 16
                color: Material.accent
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 10
                text: qsTr("Tips for lowest latency: use a wired connection for the console (and PC if you can), and keep V-Sync off in Settings.")
                font.pixelSize: 14
                opacity: 0.6
            }
        }
    }
}
