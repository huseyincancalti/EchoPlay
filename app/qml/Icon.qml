import QtQuick
import "Icons.js" as Icons

// A Material Symbols Rounded glyph from the cut-down icon font (app/fonts/icons.txt, tools/make-fonts.sh).
Text {
    property string name
    property bool filled: false
    property int size: 24

    text: Icons.glyph[name] || ""
    font.family: filled ? "EchoPlay Icons Filled" : "EchoPlay Icons"
    font.pixelSize: size
    renderType: Text.NativeRendering // the default distance field breaks the filled glyphs' overlapping contours
    width: size
    height: size
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}
