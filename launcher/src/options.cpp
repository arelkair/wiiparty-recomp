#include "options.h"

#include "texts.h"

namespace {

Option toggle(const char* key, const char* fallback, const QString& group, const QString& label, const QString& detail) {
    return Option{key, Option::Kind::Toggle, fallback, group, label, detail, {}};
}

QList<Option> build() {
    const Texts& t = texts();
    QList<QPair<QString, QString>> scales = {{"1", t.scale_native}, {"2", "2x"}, {"3", "3x"}, {"4", "4x"}, {"5", "5x"}, {"6", "6x"}};
    QList<QPair<QString, QString>> languages = {{"auto", t.language_auto}, {"en", "English"},  {"de", "Deutsch"},    {"fr", "Français"},
                                                {"es", "Español"},         {"it", "Italiano"}, {"nl", "Nederlands"}};
    return {
        Option{"video.scale", Option::Kind::Choice, "1", t.group_video, t.scale, t.scale_detail, scales},
        toggle("video.fullscreen", "0", t.group_video, t.fullscreen, t.fullscreen_detail),
        toggle("video.copy_filter", "1", t.group_video, t.copy_filter, t.copy_filter_detail),
        toggle("input.gamepads", "1", t.group_input, t.gamepads, t.gamepads_detail),
        toggle("input.auto_grip", "1", t.group_input, t.auto_grip, t.auto_grip_detail),
        toggle("input.wake_on_mouse", "1", t.group_input, t.wake_on_mouse, t.wake_on_mouse_detail),
        toggle("input.hide_cursor", "0", t.group_input, t.hide_cursor, t.hide_cursor_detail),
        Option{"system.language", Option::Kind::Choice, "auto", t.group_system, t.language, t.language_detail, languages},
        toggle("system.pal60", "1", t.group_system, t.pal60, t.pal60_detail),
        toggle("system.skip_notices", "1", t.group_system, t.skip_notices, t.skip_notices_detail),
        toggle("audio.mute", "0", t.group_audio, t.mute, t.mute_detail),
    };
}

}

const QList<Option>& options() {
    static const QList<Option> value = build();
    return value;
}
