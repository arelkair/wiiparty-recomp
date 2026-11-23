#pragma once

namespace wp::layout {

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

inline Rect compose(double client_width, double client_height, double aspect) {
    Rect frame;
    frame.width = client_width;
    frame.height = frame.width / aspect;
    if (frame.height > client_height) {
        frame.height = client_height;
        frame.width = frame.height * aspect;
    }
    frame.x = (client_width - frame.width) / 2;
    frame.y = (client_height - frame.height) / 2;
    return frame;
}

inline double composed_aspect(double client_width, double client_height, double aspect) {
    if (client_width <= 0.0 || client_height <= 0.0 || aspect <= 0.0) {
        return aspect;
    }
    Rect frame = compose(client_width, client_height, aspect);
    return frame.height > 0.0 ? frame.width / frame.height : aspect;
}

}
