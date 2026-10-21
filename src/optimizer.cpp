#include "optimizer.h"

#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <limits>

namespace pcforge {

Preferences Preferences::fromPrompt(const QString &description, double defaultBudget) {
    Preferences p;
    p.budget = defaultBudget;
    const auto s = description.toLower();
    if (s.contains("gaming") || s.contains("game") || s.contains("fps") ||
        s.contains("call of duty") || s.contains("cod")) p.focus = "gaming";
    else if (s.contains("workstation") || s.contains("compile") || s.contains("programming") ||
             s.contains("render") || s.contains("development") || s.contains("chess engine"))
        p.focus = "workstation";
    p.quiet = s.contains("quiet") || s.contains("silent") || s.contains("low noise");
    QRegularExpression money(R"(\$\s*([\d,]+(?:\.\d+)?)|([\d,]+(?:\.\d+)?)\s*(?:usd|dollars))",
                             QRegularExpression::CaseInsensitiveOption);
    auto match = money.match(description);
    if (match.hasMatch()) {
        QString raw = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        raw.remove(',');
        const double parsed = raw.toDouble();
        if (std::isfinite(parsed) && parsed > 0) p.budget = parsed;
    }
    return p;
}

Proposal generateBuild(const QVector<Part> &catalog, const Preferences &pref) {
    Proposal response;
    if (!std::isfinite(pref.budget) || pref.budget <= 0) {
        response.error = "Please enter a positive budget.";
        return response;
    }
    struct Candidate { Build build; double price = 0; double score = 0; };
    QVector<Candidate> beam(1);
    constexpr int beamWidth = 200;
    for (const QString &category : categories()) {
        QVector<Candidate> next;
        bool haveParts = false;
        for (const auto &candidate : beam) {
            for (const auto &part : catalog) {
                if (part.category != category || !std::isfinite(part.price) || part.price < 0) continue;
                haveParts = true;
                if (candidate.price + part.price > pref.budget + 0.001) continue;
                Candidate addition = candidate;
                addition.build.selected.insert(category, part.id);
                if (!validate(addition.build, catalog).compatible()) continue;
                addition.price += part.price;
                double weight = 1.0;
                if (pref.focus == "gaming") {
                    if (category == "GPU") weight = 2.7;
                    if (category == "CPU") weight = 1.65;
                    if (category == "Memory") weight = 0.9;
                } else if (pref.focus == "workstation") {
                    if (category == "CPU") weight = 2.7;
                    if (category == "Memory") weight = 1.9;
                    if (category == "GPU") weight = 0.8;
                    if (category == "Storage") weight = 1.25;
                }
                addition.score += std::clamp(part.performance, 0, 100) * weight -
                                  part.price * 0.006;
                if (pref.quiet && category == "Cooler") addition.score += part.performance * 0.25;
                next.push_back(std::move(addition));
            }
        }
        if (!haveParts) {
            response.error = "No " + category + " components in the catalog.";
            return response;
        }
        if (next.isEmpty()) {
            response.error = "No compatible " + category +
                " option fits the budget and earlier choices. Add more parts or raise the budget.";
            return response;
        }
        std::sort(next.begin(), next.end(), [](const Candidate &a, const Candidate &b) {
            if (a.score != b.score) return a.score > b.score;
            return a.price < b.price;
        });
        if (next.size() > beamWidth) next.resize(beamWidth);
        beam = std::move(next);
    }
    for (const auto &candidate : beam) {
        if (!validate(candidate.build, catalog, true).compatible()) continue;
        response.build = candidate.build;
        response.build.name = pref.focus == "gaming" ? "Gaming Build" :
                              pref.focus == "workstation" ? "Workstation Build" : "Balanced Build";
        response.total = candidate.price;
        response.notes << "Generated offline from the local catalog, not live retailer inventory."
                       << "Performance scores and example prices are estimates supplied in the catalog."
                       << "Compatibility covers key fields only; manually verify BIOS, connectors and physical fit.";
        if (pref.quiet) response.notes << "Quiet preference is a heuristic, not a measured noise rating.";
        return response;
    }
    response.error = "No fully compatible build found within the budget.";
    return response;
}
