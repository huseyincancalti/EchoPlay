import QtQuick
import EchoPlay

// Text in the theme's font. Setting QGuiApplication's font would not reach Text items that already exist.
Text {
    font.family: Theme.fontFamily
    font.pixelSize: Theme.tokens.type.body
    renderType: Text.NativeRendering
    elide: Text.ElideRight
}
