import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: root
    preferredRepresentation: fullRepresentation
    implicitWidth: Kirigami.Units.gridUnit * 19
    implicitHeight: Kirigami.Units.gridUnit * 20
    property int monthOffset: 0
    property date now: new Date()

    readonly property int viewYear: new Date(now.getFullYear(), now.getMonth() + monthOffset, 1).getFullYear()
    readonly property int viewMonth: new Date(now.getFullYear(), now.getMonth() + monthOffset, 1).getMonth()
    readonly property int firstMonday: (new Date(viewYear, viewMonth, 1).getDay() + 6) % 7
    readonly property int daysInMonth: new Date(viewYear, viewMonth + 1, 0).getDate()
    readonly property string monthLabel: Qt.locale().monthName(viewMonth, Locale.LongFormat).toUpperCase()
        + " " + viewYear

    function changeMonth(delta) {
        root.monthOffset += delta;
    }

    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: root.now = new Date()
    }

    fullRepresentation: Item {
        id: page
        implicitWidth: root.implicitWidth
        implicitHeight: root.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: 22
            color: "#D6101116"
            border.width: 1
            border.color: "#38FFFFFF"
        }

        Item {
            id: content
            anchors.fill: parent
            anchors.margins: 20

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top

                Text {
                    text: "CALENDAR"
                    color: "#B4B0BA"
                    font.family: "Inter"
                    font.pixelSize: 11
                    font.letterSpacing: 1
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "SOLSTICE"
                    color: "#D6B878"
                    font.family: "Inter"
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }
            }

            Item {
                id: dateRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 30
                height: 76

                Text {
                    text: root.now.getDate()
                    color: "#F4F0E7"
                    font.family: "Playfair Display"
                    font.pixelSize: 66
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 60
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: Qt.locale().dayName(root.now.getDay(), Locale.LongFormat).toUpperCase()
                        color: "#E4DFD5"
                        font.family: "Inter"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.monthLabel
                        color: "#8F8B96"
                        font.family: "Inter"
                        font.pixelSize: 10
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    spacing: 4
                    Repeater {
                        model: [
                            { label: "‹", delta: -1, tip: "Previous month" },
                            { label: "›", delta: 1, tip: "Next month" }
                        ]
                        delegate: Item {
                            width: 26
                            height: 26
                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: mouse.containsMouse ? "#D6B878" : "#B4B0BA"
                                font.family: "Inter"
                                font.pixelSize: 20
                            }
                            MouseArea {
                                id: mouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: modelData.tip
                                onClicked: root.changeMonth(modelData.delta)
                            }
                        }
                    }
                }
            }

            Grid {
                id: weekHeader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: dateRow.bottom
                anchors.topMargin: 8
                columns: 7
                Repeater {
                    model: ["M", "T", "W", "T", "F", "S", "S"]
                    delegate: Text {
                        width: weekHeader.width / 7
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: "#807D88"
                        font.family: "Inter"
                        font.pixelSize: 10
                    }
                }
            }

            Grid {
                id: monthGrid
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: weekHeader.bottom
                anchors.topMargin: 5
                columns: 7
                Repeater {
                    model: 42
                    delegate: Item {
                        width: monthGrid.width / 7
                        height: 23
                        readonly property int dayNumber: index - root.firstMonday + 1
                        readonly property bool inMonth: dayNumber >= 1 && dayNumber <= root.daysInMonth
                        readonly property bool today: root.monthOffset === 0
                            && dayNumber === root.now.getDate()

                        Rectangle {
                            anchors.centerIn: parent
                            width: 23
                            height: 19
                            radius: 6
                            color: parent.today ? "#D6B878" : "transparent"
                        }
                        Text {
                            anchors.centerIn: parent
                            text: parent.inMonth ? parent.dayNumber : ""
                            color: parent.today ? "#14151C" : (parent.inMonth ? "#C9C4B9" : "#4A4B55")
                            font.family: "Inter"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }
    }
}
