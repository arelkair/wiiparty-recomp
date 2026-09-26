#pragma once

#include <QString>

class Project {
public:
    static Project locate(const QString& game);

    bool valid() const;
    QString root() const;
    QString game() const;
    QString title() const;
    QString game_folder() const;
    QString extracted_folder() const;
    QString settings_file() const;
    QString executable() const;
    bool extracted() const;
    bool built() const;

private:
    QString root_;
    QString game_;
    QString title_;
};
