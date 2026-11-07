import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    width: 1920
    height: 1080
    color: "#090A0E"

    property int sessionIndex: sessionModel.lastIndex
    property string errorText: ""

    Image {
        anchors.fill: parent
        source: "background.png"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }

    Rectangle {
        anchors.fill: parent
        color: "#66090A0E"
    }

    Column {
        id: panel
        anchors.centerIn: parent
        width: 360
        spacing: 12

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "AURELIA"
            color: "#D6B878"
            font.family: "Playfair Display"
            font.pixelSize: 48
            font.letterSpacing: 2
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "SOLSTICE  ·  1.0"
            color: "#B4B0BA"
            font.family: "Inter"
            font.pixelSize: 11
            font.letterSpacing: 3
            bottomPadding: 24
        }

        TextField {
            id: userName
            width: parent.width
            text: userModel.lastUser
            placeholderText: "User name"
            font.family: "Inter"
            font.pixelSize: 14
            color: "#F4F0E7"
            placeholderTextColor: "#85838A"
            background: Rectangle {
                implicitHeight: 44
                radius: 8
                color: "#CC14151C"
                border.width: userName.activeFocus ? 1 : 0
                border.color: "#D6B878"
            }
            Keys.onReturnPressed: password.forceActiveFocus()
        }

        TextField {
            id: password
            width: parent.width
            echoMode: TextInput.Password
            placeholderText: "Password"
            font.family: "Inter"
            font.pixelSize: 14
            color: "#F4F0E7"
            placeholderTextColor: "#85838A"
            background: Rectangle {
                implicitHeight: 44
                radius: 8
                color: "#CC14151C"
                border.width: password.activeFocus ? 1 : 0
                border.color: "#D6B878"
            }
            Keys.onReturnPressed: root.doLogin()
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            height: 18
            text: root.errorText
            color: "#E07878"
            font.family: "Inter"
            font.pixelSize: 11
        }

        Button {
            id: loginButton
            width: parent.width
            height: 44
            text: "Sign in"
            font.family: "Inter"
            font.pixelSize: 13
            onClicked: root.doLogin()
            contentItem: Text {
                text: loginButton.text
                color: "#14151C"
                font: loginButton.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 8
                color: loginButton.pressed ? "#C2A568" : (loginButton.hovered ? "#E2C48D" : "#D6B878")
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "KDE Plasma  ·  Wayland"
            color: "#706E79"
            font.family: "Inter"
            font.pixelSize: 10
            topPadding: 14
        }
    }

    Connections {
        target: sddm
        function onLoginFailed() {
            root.errorText = "Unable to sign in. Check the user name and password.";
            password.text = "";
            password.forceActiveFocus();
        }
    }

    function doLogin() {
        root.errorText = "";
        sddm.login(userName.text, password.text, root.sessionIndex);
    }

    Component.onCompleted: {
        if (password.text.length === 0) {
            password.forceActiveFocus();
        }
    }
}
