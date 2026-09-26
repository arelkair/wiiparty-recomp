#include "key_capture.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QWheelEvent>

#include "texts.h"
#include "wp/keymap.h"

namespace {

#ifdef Q_OS_WIN
constexpr quint32 kLeftShiftScan = 0x2A;
constexpr quint32 kRightShiftScan = 0x36;
constexpr quint32 kShiftKey = 0x10;
#else
constexpr quint32 kX11LeftShift = 0xFFE1;
constexpr quint32 kX11RightShift = 0xFFE2;
#endif

QString known(const std::string& name) {
    return wp::keymap::key_code(name) >= 0 ? QString::fromStdString(name) : QString();
}

QString shift_name(const QKeyEvent* event) {
#ifdef Q_OS_WIN
    if (event->nativeScanCode() == kLeftShiftScan) {
        return "LeftShift";
    }
    if (event->nativeScanCode() == kRightShiftScan) {
        return "RightShift";
    }
#else
    if (event->nativeVirtualKey() == kX11LeftShift) {
        return "LeftShift";
    }
    if (event->nativeVirtualKey() == kX11RightShift) {
        return "RightShift";
    }
#endif
    return "Shift";
}

QString qt_key_name(const QKeyEvent* event) {
    int key = event->key();
    bool keypad = event->modifiers() & Qt::KeypadModifier;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QString(QChar('A' + (key - Qt::Key_A)));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return (keypad ? "Num" : "") + QString(QChar('0' + (key - Qt::Key_0)));
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return "F" + QString::number(key - Qt::Key_F1 + 1);
    }
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return "Enter";
    case Qt::Key_Space:
        return "Space";
    case Qt::Key_Backspace:
        return "Backspace";
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return "Tab";
    case Qt::Key_Shift:
        return shift_name(event);
    case Qt::Key_Control:
        return "Ctrl";
    case Qt::Key_Alt:
        return "Alt";
    case Qt::Key_AltGr:
        return "RightAlt";
    case Qt::Key_Up:
        return "Up";
    case Qt::Key_Down:
        return "Down";
    case Qt::Key_Left:
        return "Left";
    case Qt::Key_Right:
        return "Right";
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        return keypad ? "NumPlus" : "Plus";
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        return keypad ? "NumMinus" : "Minus";
    case Qt::Key_Asterisk:
        return keypad ? "NumMultiply" : QString();
    case Qt::Key_Slash:
        return keypad ? "NumDivide" : "Slash";
    case Qt::Key_Period:
        return keypad ? "NumPeriod" : "Period";
    case Qt::Key_Comma:
        return "Comma";
    case Qt::Key_Semicolon:
        return "Semicolon";
    case Qt::Key_Apostrophe:
        return "Quote";
    case Qt::Key_QuoteLeft:
        return "Backquote";
    case Qt::Key_BracketLeft:
        return "LeftBracket";
    case Qt::Key_BracketRight:
        return "RightBracket";
    case Qt::Key_Backslash:
        return "Backslash";
    case Qt::Key_PageUp:
        return "PageUp";
    case Qt::Key_PageDown:
        return "PageDown";
    case Qt::Key_Home:
        return "Home";
    case Qt::Key_End:
        return "End";
    case Qt::Key_Insert:
        return "Insert";
    case Qt::Key_Delete:
        return "Delete";
    case Qt::Key_Pause:
        return "Pause";
    case Qt::Key_CapsLock:
        return "CapsLock";
    default:
        return QString();
    }
}

QString mouse_name(Qt::MouseButton button) {
    switch (button) {
    case Qt::LeftButton:
        return "MouseLeft";
    case Qt::RightButton:
        return "MouseRight";
    case Qt::MiddleButton:
        return "MouseMiddle";
    case Qt::BackButton:
        return "MouseX1";
    case Qt::ForwardButton:
        return "MouseX2";
    default:
        return QString();
    }
}

}

QString key_event_name(const QKeyEvent* event) {
#ifdef Q_OS_WIN
    quint32 code = event->nativeVirtualKey();
    if (code == kShiftKey) {
        return shift_name(event);
    }
    if (code != 0) {
        QString name = known(wp::keymap::key_name(static_cast<int>(code)));
        if (!name.isEmpty()) {
            return name;
        }
    }
#endif
    return known(qt_key_name(event).toStdString());
}

KeyCaptureButton::KeyCaptureButton(QWidget* parent) : QPushButton(parent) {
    setObjectName("binding");
    setCursor(Qt::PointingHandCursor);
    connect(this, &QPushButton::clicked, this, &KeyCaptureButton::start);
}

void KeyCaptureButton::set_binding(const QString& text) {
    binding_ = text;
    if (!capturing_) {
        setText(binding_);
    }
}

void KeyCaptureButton::start() {
    capturing_ = true;
    setProperty("capturing", true);
    style()->unpolish(this);
    style()->polish(this);
    setText(texts().keys_press);
    setFocus(Qt::MouseFocusReason);
}

void KeyCaptureButton::stop() {
    capturing_ = false;
    setProperty("capturing", false);
    style()->unpolish(this);
    style()->polish(this);
    setText(binding_);
}

void KeyCaptureButton::finish(const QString& name) {
    stop();
    emit captured(name);
}

void KeyCaptureButton::keyPressEvent(QKeyEvent* event) {
    if (!capturing_) {
        QPushButton::keyPressEvent(event);
        return;
    }
    event->accept();
    if (event->isAutoRepeat()) {
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        stop();
        return;
    }
    QString name = key_event_name(event);
    if (name.isEmpty()) {
        return;
    }
    if (wp::keymap::reserved(wp::keymap::key_code(name.toStdString()))) {
        setText(texts().keys_reserved);
        return;
    }
    finish(name);
}

void KeyCaptureButton::mousePressEvent(QMouseEvent* event) {
    if (!capturing_) {
        QPushButton::mousePressEvent(event);
        return;
    }
    event->accept();
    QString name = mouse_name(event->button());
    if (!name.isEmpty()) {
        swallow_release_ = true;
        finish(name);
    }
}

void KeyCaptureButton::mouseReleaseEvent(QMouseEvent* event) {
    if (capturing_ || swallow_release_) {
        swallow_release_ = false;
        event->accept();
        return;
    }
    QPushButton::mouseReleaseEvent(event);
}

void KeyCaptureButton::wheelEvent(QWheelEvent* event) {
    if (!capturing_ || event->angleDelta().y() == 0) {
        QPushButton::wheelEvent(event);
        return;
    }
    event->accept();
    finish(event->angleDelta().y() > 0 ? "WheelUp" : "WheelDown");
}

void KeyCaptureButton::focusOutEvent(QFocusEvent* event) {
    if (capturing_) {
        stop();
    }
    QPushButton::focusOutEvent(event);
}

bool KeyCaptureButton::focusNextPrevChild(bool next) {
    if (capturing_) {
        return false;
    }
    return QPushButton::focusNextPrevChild(next);
}
