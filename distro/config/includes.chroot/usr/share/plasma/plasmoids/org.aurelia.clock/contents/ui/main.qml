import QtQuick
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: root
    preferredRepresentation: fullRepresentation
    implicitWidth: Kirigami.Units.gridUnit * 42
    implicitHeight: Kirigami.Units.gridUnit * 22

    property date now: new Date()
    readonly property string timeText: two(now.getHours()) + ":" + two(now.getMinutes())
    readonly property string weekdayText: Qt.locale().dayName(now.getDay(), Locale.LongFormat).toUpperCase()
    readonly property string dateText: now.getDate() + " "
        + Qt.locale().monthName(now.getMonth(), Locale.LongFormat).toUpperCase() + " "
        + now.getFullYear()

    function two(value) {
        return value < 10 ? "0" + value : "" + value;
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.now = new Date()
    }

    fullRepresentation: Item {
        id: page
        implicitWidth: root.implicitWidth
        implicitHeight: root.implicitHeight

        Column {
            anchors.centerIn: parent
            spacing: 2

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 44
                height: 1
                color: "#D6B878"
                opacity: 0.48
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "SOLSTICE"
                color: "#D6B878"
                opacity: 0.9
                font.family: "Inter"
                font.pixelSize: 12
                font.letterSpacing: 5
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.weekdayText
                color: "#F4F0E7"
                opacity: 0.84
                font.family: "Playfair Display"
                font.pixelSize: Math.max(30, Math.round(root.height * 0.18))
                font.letterSpacing: 10
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.timeText
                color: "#F4F0E7"
                font.family: "Playfair Display"
                font.pixelSize: Math.max(64, Math.round(root.height * 0.44))
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.dateText
                color: "#D6B878"
                opacity: 0.9
                font.family: "Inter"
                font.pixelSize: 13
                font.letterSpacing: 3
            }
        }
    }
}
