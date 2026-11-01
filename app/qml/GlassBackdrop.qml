import QtQuick
import QtQuick.Effects
import EchoPlay

// The video, small and blurred, once per frame for all glass panels: each panel samples its own part of it.
// Blurring per panel left a thin bar unblurred and cost a pass per panel. Runs only while a panel shows.
Item {
    id: backdrop

    required property Item source
    property bool active: false
    readonly property Item texture: result
    readonly property size frameSize: Qt.size(640, 360) // the video's detail is gone after the blur anyway

    ShaderEffectSource {
        id: frame
        visible: false
        sourceItem: backdrop.source
        textureSize: backdrop.frameSize
        live: backdrop.active
    }
    MultiEffect {
        id: blurred
        visible: false
        width: backdrop.frameSize.width
        height: backdrop.frameSize.height
        source: frame
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1
        // The token is a CSS blur on a 1280 wide window. MultiEffect's radius reaches about half as far on this
        // half-size frame, so the look needs about twice the token (checked against the boards).
        blurMax: Math.min(64, Theme.tokens.blur.panel * 2.3)
    }
    ShaderEffectSource {
        id: result
        visible: false
        sourceItem: blurred
        textureSize: backdrop.frameSize
        live: backdrop.active
    }
}
