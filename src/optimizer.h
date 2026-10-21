#pragma once
#include "core.h"

namespace pcforge {

struct Preferences {
    double budget = 2500;
    QString focus = "balanced"; // balanced, gaming, workstation
    bool quiet = false;
    static Preferences fromPrompt(const QString &description, double defaultBudget = 2500);
};

struct Proposal {
    Build build;
    double total = 0;
    QString error;
    QStringList notes;
    bool ok() const { return error.isEmpty(); }
};

Proposal generateBuild(const QVector<Part> &catalog, const Preferences &preferences);

} // namespace pcforge
