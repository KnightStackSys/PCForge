#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace pcforge {

QStringList categories();
QString normalizeCategory(const QString &value);

struct Part {
    QString id;
    QString category;
    QString name;
    double price = 0.0;
    int performance = 50;
    QJsonObject specs;
    QString source = "custom";

    QJsonObject toJson() const;
    static Part fromJson(const QJsonObject &json);
};

struct Build {
    QString name = "Untitled Build";
    QMap<QString, QString> selected; // category -> part ID

    QJsonObject toJson() const;
    static Build fromJson(const QJsonObject &json);
};

class Catalog {
public:
    QVector<Part> parts;
    const Part *find(const QString &id) const;
    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    void seedExamples();
};

struct Validation {
    QStringList errors;
    QStringList warnings;
    bool compatible() const { return errors.isEmpty(); }
};

Validation validate(const Build &build, const QVector<Part> &parts, bool requireComplete = false);
double totalPrice(const Build &build, const QVector<Part> &parts);
QVector<Part> importPCPartPickerText(const QString &text);

} // namespace pcforge
