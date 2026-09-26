#pragma once

#include <QPushButton>

class KeyCaptureButton : public QPushButton {
    Q_OBJECT

public:
    explicit KeyCaptureButton(QWidget* parent = nullptr);

    void set_binding(const QString& text);
    bool capturing() const { return capturing_; }

signals:
    void captured(const QString& name);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void start();
    void stop();
    void finish(const QString& name);

    QString binding_;
    bool capturing_ = false;
    bool swallow_release_ = false;
};

QString key_event_name(const QKeyEvent* event);
