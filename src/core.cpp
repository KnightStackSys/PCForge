#include "core.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace pcforge {
namespace {
QString lower(const QString &s) { return s.trimmed().toLower(); }
bool matches(const QString &list, const QString &item) {
    for (const auto &v : list.split(',', Qt::SkipEmptyParts))
        if (lower(v) == lower(item)) return true;
    return false;
}
QString spec(const Part *p, const char *key) {
    return p ? p->specs.value(QLatin1String(key)).toString().trimmed() : QString{};
}
double number(const Part *p, const char *key) {
    return p ? p->specs.value(QLatin1String(key)).toDouble(-1) : -1;
}
QString money(double n) { return QString::number(n, 'f', 2); }
}

QStringList categories() {
    return {"CPU", "Motherboard", "Memory", "GPU", "Storage", "PSU", "Case", "Cooler"};
}

QString normalizeCategory(const QString &value) {
    const auto v = lower(value);
    if (v == "cpu" || v == "processor") return "CPU";
    if (v == "motherboard" || v == "mobo") return "Motherboard";
    if (v == "memory" || v == "ram") return "Memory";
    if (v == "gpu" || v == "video card" || v == "graphics card") return "GPU";
    if (v == "storage" || v == "ssd" || v == "hard drive") return "Storage";
    if (v == "psu" || v == "power supply") return "PSU";
    if (v == "case") return "Case";
    if (v == "cooler" || v == "cpu cooler") return "Cooler";
    return {};
}

QJsonObject Part::toJson() const {
    return {{"id", id}, {"category", category}, {"name", name},
            {"price", price}, {"performance", performance},
            {"specs", specs}, {"source", source}};
}
Part Part::fromJson(const QJsonObject &o) {
    Part p;
    p.id = o.value("id").toString();
    p.category = normalizeCategory(o.value("category").toString());
    p.name = o.value("name").toString().trimmed();
    p.price = o.value("price").toDouble();
    p.performance = o.value("performance").toInt(50);
    p.specs = o.value("specs").toObject();
    p.source = o.value("source").toString("custom");
    return p;
}
QJsonObject Build::toJson() const {
    QJsonObject choices;
    for (auto i = selected.cbegin(); i != selected.cend(); ++i) choices.insert(i.key(), i.value());
    return {{"name", name}, {"selected", choices}};
}
Build Build::fromJson(const QJsonObject &o) {
    Build b;
    b.name = o.value("name").toString("Untitled Build");
    const auto values = o.value("selected").toObject();
    for (auto i = values.begin(); i != values.end(); ++i) {
        const auto type = normalizeCategory(i.key());
        if (!type.isEmpty() && i.value().isString()) b.selected[type] = i.value().toString();
    }
    return b;
}
const Part *Catalog::find(const QString &id) const {
    for (const auto &p : parts) if (p.id == id) return &p;
    return nullptr;
}
bool Catalog::load(const QString &path, QString *error) {
    QFile file(path);
    if (!file.exists()) {
        seedExamples();
        return save(path, error);
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject() ||
        !doc.object().value("parts").isArray()) {
        if (error) *error = "Invalid catalog JSON: " + parseError.errorString();
        return false;
    }
    QVector<Part> next;
    for (const auto &v : doc.object().value("parts").toArray()) {
        if (!v.isObject()) continue;
        auto p = Part::fromJson(v.toObject());
        if (p.id.isEmpty() || p.category.isEmpty() || p.name.isEmpty() ||
            !std::isfinite(p.price) || p.price < 0) continue;
        next.push_back(p);
    }
    parts = next;
    return true;
}
bool Catalog::save(const QString &path, QString *error) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonArray array;
    for (const auto &p : parts) array.push_back(p.toJson());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{"format", 1}, {"parts", array}}).toJson());
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
void Catalog::seedExamples() {
    parts.clear();
    auto add = [this](QString type, QString name, double price, int perf, QJsonObject specs) {
        parts.push_back({QUuid::createUuid().toString(QUuid::WithoutBraces),
                         type, name, price, perf, specs, "example"});
    };
    // Illustrative entries and prices only, not real-time retail offers.
    add("CPU", "Sample AM5 8-Core", 330, 80, {{"socket","AM5"},{"power_w",105}});
    add("CPU", "Sample AM5 16-Core", 590, 96, {{"socket","AM5"},{"power_w",170}});
    add("CPU", "Sample LGA1700 12-Core", 300, 75, {{"socket","LGA1700"},{"power_w",125}});
    add("Motherboard", "Sample B650 ATX DDR5", 180, 65, {{"socket","AM5"},{"ram_type","DDR5"},{"form_factor","ATX"}});
    add("Motherboard", "Sample X670 ATX DDR5", 300, 82, {{"socket","AM5"},{"ram_type","DDR5"},{"form_factor","ATX"}});
    add("Motherboard", "Sample B760 mATX DDR5", 175, 65, {{"socket","LGA1700"},{"ram_type","DDR5"},{"form_factor","mATX"}});
    add("Memory", "Sample 32GB DDR5", 120, 70, {{"ram_type","DDR5"},{"capacity_gb",32}});
    add("Memory", "Sample 64GB DDR5", 220, 88, {{"ram_type","DDR5"},{"capacity_gb",64}});
    add("GPU", "Sample 8GB Graphics Card", 300, 55, {{"power_w",180},{"gpu_length_mm",240}});
    add("GPU", "Sample 16GB Graphics Card", 680, 83, {{"power_w",285},{"gpu_length_mm",310}});
    add("GPU", "Sample 24GB Graphics Card", 1650, 99, {{"power_w",450},{"gpu_length_mm",340}});
    add("Storage", "Sample 1TB NVMe SSD", 90, 60, {{"capacity_gb",1000}});
    add("Storage", "Sample 2TB NVMe SSD", 170, 85, {{"capacity_gb",2000}});
    add("PSU", "Sample 750W ATX PSU", 115, 65, {{"capacity_w",750}});
    add("PSU", "Sample 1000W ATX PSU", 185, 85, {{"capacity_w",1000}});
    add("PSU", "Sample 1200W ATX PSU", 245, 90, {{"capacity_w",1200}});
    add("Case", "Sample ATX Mid-Tower", 110, 70,
        {{"form_factors","ATX,mATX,Mini-ITX"},{"max_gpu_length_mm",330},{"max_cooler_height_mm",165}});
    add("Case", "Sample ATX Full-Tower", 195, 80,
        {{"form_factors","ATX,mATX,Mini-ITX"},{"max_gpu_length_mm",400},{"max_cooler_height_mm",195}});
    add("Cooler", "Sample 160mm Dual-Tower Air", 90, 78,
        {{"sockets","AM5,LGA1700"},{"cooler_height_mm",160}});
    add("Cooler", "Sample 140mm Air", 55, 60,
        {{"sockets","AM5,LGA1700"},{"cooler_height_mm",140}});
}
double totalPrice(const Build &b, const QVector<Part> &parts) {
    double result = 0;
    for (const auto &p : parts)
        if (b.selected.value(p.category) == p.id) result += p.price;
    return result;
}
Validation validate(const Build &b, const QVector<Part> &parts, bool requireComplete) {
    Validation v;
    QMap<QString, const Part *> chosen;
    for (auto it = b.selected.cbegin(); it != b.selected.cend(); ++it) {
        const Part *found = nullptr;
        for (const auto &p : parts) if (p.id == it.value()) { found = &p; break; }
        if (!found) { v.errors << "Missing catalog item for " + it.key(); continue; }
        if (found->category != it.key()) {
            v.errors << "Category mismatch for " + found->name; continue;
        }
        chosen[it.key()] = found;
    }
    if (requireComplete) for (const auto &c : categories())
        if (!chosen.contains(c)) v.errors << "No " + c + " selected";

    auto compare = [&](const QString &leftType, const char *leftKey,
                       const QString &rightType, const char *rightKey, const QString &desc) {
        auto *a = chosen.value(leftType, nullptr), *d = chosen.value(rightType, nullptr);
        if (!a || !d) return;
        auto left = spec(a, leftKey), right = spec(d, rightKey);
        if (!left.isEmpty() && !right.isEmpty() && lower(left) != lower(right))
            v.errors << desc + ": " + left + " vs " + right;
        else if (left.isEmpty() || right.isEmpty())
            v.warnings << "Cannot verify " + desc + " (missing specifications)";
    };
    compare("CPU", "socket", "Motherboard", "socket", "CPU/motherboard socket");
    compare("Memory", "ram_type", "Motherboard", "ram_type", "RAM/motherboard type");

    const Part *board = chosen.value("Motherboard", nullptr), *box = chosen.value("Case", nullptr);
    if (board && box) {
        auto form = spec(board, "form_factor"), supported = spec(box, "form_factors");
        if (!form.isEmpty() && !supported.isEmpty() && !matches(supported, form))
            v.errors << "Motherboard form factor is not supported by the case";
        else if (form.isEmpty() || supported.isEmpty())
            v.warnings << "Cannot verify motherboard/case fit";
    }
    const Part *cpu = chosen.value("CPU", nullptr), *cooler = chosen.value("Cooler", nullptr);
    if (cpu && cooler) {
        const auto socket = spec(cpu, "socket"), supported = spec(cooler, "sockets");
        if (!socket.isEmpty() && !supported.isEmpty() && !matches(supported, socket))
            v.errors << "Cooler does not support the CPU socket";
        else if (socket.isEmpty() || supported.isEmpty())
            v.warnings << "Cannot verify cooler socket support";
    }
    const Part *gpu = chosen.value("GPU", nullptr);
    if (gpu && box) {
        double length = number(gpu, "gpu_length_mm"), maximum = number(box, "max_gpu_length_mm");
        if (length > 0 && maximum > 0 && length > maximum)
            v.errors << "Graphics card is too long for the case";
        else if (length <= 0 || maximum <= 0)
            v.warnings << "Cannot verify GPU clearance";
    }
    if (cooler && box) {
        double height = number(cooler, "cooler_height_mm"), maximum = number(box, "max_cooler_height_mm");
        if (height > 0 && maximum > 0 && height > maximum)
            v.errors << "CPU cooler is too tall for the case";
        else if (height <= 0 || maximum <= 0)
            v.warnings << "Cannot verify cooler clearance";
    }
    const Part *psu = chosen.value("PSU", nullptr);
    if (psu && cpu && gpu) {
        double capacity = number(psu, "capacity_w"), cpuPower = number(cpu, "power_w"),
               gpuPower = number(gpu, "power_w");
        if (capacity > 0 && cpuPower > 0 && gpuPower > 0) {
            const double recommended = (cpuPower + gpuPower + 75) * 1.25;
            if (capacity < recommended)
                v.errors << "PSU capacity is below the estimated 25% power-headroom target (" +
                    QString::number(std::ceil(recommended)) + "W needed)";
        } else v.warnings << "Cannot verify PSU power headroom";
    }
    // Specifications here are intentionally minimal; verify BIOS, power connectors,
    // radiator mounts, memory QVL, and other physical constraints independently.
    return v;
}

QVector<Part> importPCPartPickerText(const QString &text) {
    QVector<Part> result;
    QRegularExpression link(R"(\[([^\]]+)\]\([^)]*\))");
    QRegularExpression amount(R"(\$\s*([\d,]+(?:\.\d{1,2})?))");
    for (const auto &line : text.split('\n')) {
        auto cells = line.split('|');
        if (!cells.isEmpty() && cells.first().trimmed().isEmpty()) cells.removeFirst();
        if (cells.size() < 3) continue;
        const auto type = normalizeCategory(QString(cells.at(0)).remove('*').trimmed());
        if (type.isEmpty()) continue;
        auto name = cells.at(1).trimmed();
        name.replace(link, "\\1");
        name.remove('*');
        name = name.trimmed();
        if (name.isEmpty() || lower(name) == "item") continue;
        double price = 0;
        for (int i = 2; i < cells.size(); ++i) {
            auto match = amount.match(cells.at(i));
            if (match.hasMatch()) {
                QString n = match.captured(1);
                n.remove(',');
                price = n.toDouble();
                break;
            }
        }
        result.push_back({QUuid::createUuid().toString(QUuid::WithoutBraces),
                          type, name, price, 50, {}, "pcpartpicker-import"});
    }
    return result;
}
} // namespace pcforge
