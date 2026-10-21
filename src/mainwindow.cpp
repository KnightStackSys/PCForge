#include "mainwindow.h"
#include "optimizer.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QUuid>
#include <cmath>
#include <algorithm>

using namespace pcforge;

namespace {

QString displayMoney(double amount) {
    return "$" + QString::number(amount, 'f', 2);
}

class PartDialog final : public QDialog {
public:
    explicit PartDialog(const Part *old, QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle(old ? "Edit component" : "Add custom component");
        resize(470, 340);
        auto *form = new QFormLayout(this);
        category_ = new QComboBox;
        category_->addItems(categories());
        name_ = new QLineEdit;
        price_ = new QDoubleSpinBox;
        price_->setRange(0, 1000000);
        price_->setDecimals(2);
        price_->setPrefix("$");
        performance_ = new QSpinBox;
        performance_->setRange(0, 100);
        performance_->setToolTip("Relative score used by the offline optimizer; not an objective benchmark.");
        specs_ = new QPlainTextEdit;
        specs_->setPlaceholderText(R"({"socket":"AM5","power_w":105})");
        specs_->setMinimumHeight(105);
        form->addRow("Category", category_);
        form->addRow("Name", name_);
        form->addRow("Price (USD)", price_);
        form->addRow("Performance (0-100)", performance_);
        form->addRow("Specifications (JSON)", specs_);
        auto *help = new QLabel("Examples: socket, ram_type, form_factor, form_factors, sockets, "
                                "power_w, capacity_w, gpu_length_mm, max_gpu_length_mm, "
                                "cooler_height_mm, max_cooler_height_mm.");
        help->setWordWrap(true);
        form->addRow(help);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (name_->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Missing name", "Provide a component name.");
                return;
            }
            QJsonParseError error;
            auto document = QJsonDocument::fromJson(specs_->toPlainText().trimmed().toUtf8(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                QMessageBox::warning(this, "Invalid specifications",
                                     "Specifications must be a valid JSON object. Use {} for unknown specifications.");
                return;
            }
            accept();
        });
        if (old) {
            category_->setCurrentText(old->category);
            category_->setEnabled(false); // preserve existing build references
            name_->setText(old->name);
            price_->setValue(old->price);
            performance_->setValue(old->performance);
            specs_->setPlainText(QString::fromUtf8(QJsonDocument(old->specs).toJson(QJsonDocument::Indented)));
        } else {
            performance_->setValue(50);
            specs_->setPlainText("{}");
        }
    }

    Part part(const Part *old) const {
        Part p;
        p.id = old ? old->id : QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.category = category_->currentText();
        p.name = name_->text().trimmed();
        p.price = price_->value();
        p.performance = performance_->value();
        p.specs = QJsonDocument::fromJson(specs_->toPlainText().trimmed().toUtf8()).object();
        p.source = "custom";
        return p;
    }

private:
    QComboBox *category_;
    QLineEdit *name_;
    QDoubleSpinBox *price_;
    QSpinBox *performance_;
    QPlainTextEdit *specs_;
};

QPushButton *makeButton(const QString &name, QLayout *layout) {
    auto *button = new QPushButton(name);
    layout->addWidget(button);
    return button;
}

bool writeBuild(const QString &path, const Build &b, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(b.toJson()).toJson()) < 0 || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("PCForge | Desktop PC Builder");
    resize(1380, 820);
    setMinimumSize(1050, 650);
    const auto appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    catalogPath_ = appData + "/catalog.json";
    autosavePath_ = appData + "/last-build.json";
    QString error;
    if (!catalog_.load(catalogPath_, &error))
        QMessageBox::warning(this, "Catalog load failed",
                             error + "\nYour existing catalog was not overwritten. Changes may not save.");

    QFile autosave(autosavePath_);
    if (autosave.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(autosave.readAll());
        if (doc.isObject()) build_ = Build::fromJson(doc.object());
    }

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    setCentralWidget(splitter);

    auto *library = new QWidget;
    auto *left = new QVBoxLayout(library);
    left->addWidget(new QLabel("<h2>Hardware Library</h2>"));
    filter_ = new QComboBox;
    filter_->addItem("All Categories");
    filter_->addItems(categories());
    search_ = new QLineEdit;
    search_->setPlaceholderText("Search components...");
    left->addWidget(filter_);
    left->addWidget(search_);
    parts_ = new QListWidget;
    parts_->setAlternatingRowColors(true);
    left->addWidget(parts_, 1);
    auto *partButtons = new QHBoxLayout;
    auto *add = makeButton("+ Add", partButtons);
    auto *edit = makeButton("Edit", partButtons);
    auto *remove = makeButton("Delete", partButtons);
    left->addLayout(partButtons);
    auto *select = makeButton("Use selected part in build →", left);
    auto *import = makeButton("Import PCPartPicker List", left);
    splitter->addWidget(library);

    auto *buildPanel = new QWidget;
    auto *center = new QVBoxLayout(buildPanel);
    center->addWidget(new QLabel("<h2>Current Build</h2>"));
    buildTable_ = new QTableWidget(categories().size(), 3);
    buildTable_->setHorizontalHeaderLabels({"Component", "Selected Hardware", "Price"});
    buildTable_->verticalHeader()->hide();
    buildTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    buildTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    buildTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    buildTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    buildTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    center->addWidget(buildTable_, 1);
    total_ = new QLabel;
    total_->setObjectName("totalPrice");
    center->addWidget(total_);
    auto *buildButtons = new QHBoxLayout;
    auto *clear = makeButton("Clear", buildButtons);
    auto *unassign = makeButton("Remove Part", buildButtons);
    auto *open = makeButton("Open Build", buildButtons);
    auto *save = makeButton("Save Build", buildButtons);
    center->addLayout(buildButtons);
    center->addWidget(new QLabel("<b>Compatibility report</b>"));
    report_ = new QPlainTextEdit;
    report_->setReadOnly(true);
    report_->setMaximumHeight(180);
    center->addWidget(report_);
    splitter->addWidget(buildPanel);

    auto *assistant = new QWidget;
    auto *right = new QVBoxLayout(assistant);
    right->addWidget(new QLabel("<h2>Build Assistant ✨</h2>"));
    auto *explain = new QLabel("Describe your ideal PC. Generate locally, or use an installed "
                             "Codex CLI for natural-language component selection.");
    explain->setWordWrap(true);
    right->addWidget(explain);
    prompt_ = new QPlainTextEdit;
    prompt_->setPlaceholderText("Example: A $2200 gaming PC for competitive FPS, "
                                "with a powerful GPU and quiet cooling.");
    prompt_->setPlainText("Build a $2200 gaming PC for competitive FPS");
    prompt_->setMaximumHeight(185);
    right->addWidget(prompt_);
    auto *budgetLabel = new QLabel("Budget (USD)");
    right->addWidget(budgetLabel);
    budget_ = new QDoubleSpinBox;
    budget_->setRange(100, 1000000);
    budget_->setDecimals(0);
    budget_->setSingleStep(100);
    budget_->setPrefix("$");
    budget_->setValue(2200);
    right->addWidget(budget_);
    auto *offline = makeButton("⚙ Generate Offline", right);
    codexButton_ = makeButton("✦ Generate with Codex CLI", right);
    auto *notice = new QLabel("Offline generation uses catalog scores, not live benchmarks. "
                             "Codex requires a separate installation and login; it is optional. "
                             "Neither mode fetches live retailer prices.");
    notice->setWordWrap(true);
    notice->setObjectName("notice");
    right->addWidget(notice);
    right->addStretch();
    splitter->addWidget(assistant);
    splitter->setSizes({340, 680, 360});

    statusBar()->showMessage("Ready — example prices are illustrative, not retail quotes.");
    setStyleSheet(R"(
        QMainWindow, QWidget { background: #151b27; color: #eef2f8; }
        QLineEdit, QListWidget, QPlainTextEdit, QTableWidget, QComboBox, QDoubleSpinBox,
        QSpinBox { background: #212b3d; color: #edf3fa; border: 1px solid #36445b;
                   border-radius: 6px; padding: 7px; selection-background-color: #376da3; }
        QPushButton { background: #2c537e; color: white; border: 1px solid #3c6796;
                      border-radius: 6px; padding: 9px 12px; }
        QPushButton:hover { background: #356aa3; }
        QPushButton:disabled { background: #465061; color: #aaa; }
        QHeaderView::section { background: #253149; color: #e8f0ff; padding: 8px; }
        QSplitter::handle { background: #36445b; }
        QLabel#totalPrice { color: #89d7aa; font-size: 21px; font-weight: bold; }
        QLabel#notice { color: #aebbd0; font-size: 11px; }
        QStatusBar { color: #b4c7e0; }
    )");

    connect(filter_, &QComboBox::currentTextChanged, this, [this] { refreshCatalog(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { refreshCatalog(); });
    connect(add, &QPushButton::clicked, this, [this] { addOrEditPart(false); });
    connect(edit, &QPushButton::clicked, this, [this] { addOrEditPart(true); });
    connect(remove, &QPushButton::clicked, this, [this] { removePart(); });
    connect(select, &QPushButton::clicked, this, [this] { assignPart(); });
    connect(parts_, &QListWidget::itemDoubleClicked, this, [this] { assignPart(); });
    connect(import, &QPushButton::clicked, this, [this] { importPartPicker(); });
    connect(clear, &QPushButton::clicked, this, [this] { newBuild(); });
    connect(unassign, &QPushButton::clicked, this, [this] { removeFromBuild(); });
    connect(open, &QPushButton::clicked, this, [this] { loadBuildFrom(); });
    connect(save, &QPushButton::clicked, this, [this] { saveBuildAs(); });
    connect(offline, &QPushButton::clicked, this, [this] { generateOffline(); });
    connect(codexButton_, &QPushButton::clicked, this, [this] { generateWithCodex(); });
    refreshCatalog();
    refreshBuild();
}

void MainWindow::message(const QString &title, const QString &body) {
    QMessageBox::information(this, title, body);
}

void MainWindow::saveCatalog() {
    QString error;
    if (!catalog_.save(catalogPath_, &error))
        QMessageBox::warning(this, "Save failed", error);
}
void MainWindow::saveSession() {
    QString error;
    if (!writeBuild(autosavePath_, build_, &error))
        statusBar()->showMessage("Autosave failed: " + error, 10000);
}
void MainWindow::refreshCatalog() {
    const auto oldId = parts_->currentItem() ? parts_->currentItem()->data(Qt::UserRole).toString() : QString{};
    parts_->clear();
    for (const auto &p : catalog_.parts) {
        if (filter_->currentIndex() > 0 && filter_->currentText() != p.category) continue;
        if (!p.name.contains(search_->text(), Qt::CaseInsensitive)) continue;
        auto *item = new QListWidgetItem(p.category + " · " + p.name + "  |  " + displayMoney(p.price));
        item->setData(Qt::UserRole, p.id);
        item->setToolTip(p.source + " — " +
                         QString::fromUtf8(QJsonDocument(p.specs).toJson(QJsonDocument::Compact)));
        parts_->addItem(item);
        if (p.id == oldId) parts_->setCurrentItem(item);
    }
}
void MainWindow::refreshBuild() {
    for (int row = 0; row < categories().size(); ++row) {
        const auto category = categories().at(row);
        const auto *p = catalog_.find(build_.selected.value(category));
        buildTable_->setItem(row, 0, new QTableWidgetItem(category));
        buildTable_->setItem(row, 1, new QTableWidgetItem(p ? p->name : "—"));
        buildTable_->setItem(row, 2, new QTableWidgetItem(p ? displayMoney(p->price) : "—"));
    }
    total_->setText("Estimated Total: " + displayMoney(totalPrice(build_, catalog_.parts)));
    auto check = validate(build_, catalog_.parts);
    QStringList lines;
    lines << "Selected: " + QString::number(build_.selected.size()) + "/8 categories";
    if (check.errors.isEmpty()) lines << "✓ No conflicts found in the available specifications.";
    for (const auto &error : check.errors) lines << "✗ " + error;
    for (const auto &warning : check.warnings) lines << "⚠ " + warning;
    if (!build_.selected.isEmpty())
        lines << "Note: Missing specifications, BIOS support, power connectors and other details "
                 "require additional verification.";
    report_->setPlainText(lines.join("\n"));
    saveSession();
}
void MainWindow::addOrEditPart(bool editing) {
    auto *item = parts_->currentItem();
    const auto id = item ? item->data(Qt::UserRole).toString() : QString{};
    auto it = std::find_if(catalog_.parts.begin(), catalog_.parts.end(),
                           [&](const Part &p) { return p.id == id; });
    if (editing && it == catalog_.parts.end()) {
        message("Select a part", "Choose a component to edit.");
        return;
    }
    const Part *old = editing ? &*it : nullptr;
    PartDialog dialog(old, this);
    if (dialog.exec() != QDialog::Accepted) return;
    Part updated = dialog.part(old);
    if (editing) *it = updated;
    else catalog_.parts.push_back(updated);
    saveCatalog();
    refreshCatalog();
    refreshBuild();
}
void MainWindow::removePart() {
    auto *item = parts_->currentItem();
    if (!item) return;
    const auto id = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, "Delete part", "Delete this component from the catalog?") !=
        QMessageBox::Yes) return;
    for (auto it = build_.selected.begin(); it != build_.selected.end();) {
        if (it.value() == id) it = build_.selected.erase(it);
        else ++it;
    }
    catalog_.parts.erase(std::remove_if(catalog_.parts.begin(), catalog_.parts.end(),
                          [&](const Part &p) { return p.id == id; }), catalog_.parts.end());
    saveCatalog();
    refreshCatalog();
    refreshBuild();
}
void MainWindow::assignPart() {
    auto *item = parts_->currentItem();
    if (!item) { message("Select a part", "Choose a component from the hardware library."); return; }
    const Part *p = catalog_.find(item->data(Qt::UserRole).toString());
    if (!p) return;
    build_.selected[p->category] = p->id;
    refreshBuild();
}
void MainWindow::removeFromBuild() {
    int row = buildTable_->currentRow();
    if (row < 0 || row >= categories().size()) return;
    build_.selected.remove(categories().at(row));
    refreshBuild();
}
void MainWindow::newBuild() {
    build_ = Build{};
    refreshBuild();
}
void MainWindow::saveBuildAs() {
    auto path = QFileDialog::getSaveFileName(this, "Save PCForge build", "pcforge-build.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    QString error;
    if (!writeBuild(path, build_, &error)) QMessageBox::warning(this, "Save failed", error);
    else statusBar()->showMessage("Saved build to " + path, 8000);
}
void MainWindow::loadBuildFrom() {
    auto path = QFileDialog::getOpenFileName(this, "Open PCForge build", {}, "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Open failed", file.errorString());
        return;
    }
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject() ||
        !doc.object().value("selected").isObject()) {
        QMessageBox::warning(this, "Invalid build", "Expected a PCForge build JSON object.");
        return;
    }
    build_ = Build::fromJson(doc.object());
    refreshBuild();
}
void MainWindow::importPartPicker() {
    QDialog dialog(this);
    dialog.setWindowTitle("Import PCPartPicker Part List");
    dialog.resize(760, 470);
    auto *layout = new QVBoxLayout(&dialog);
    auto *info = new QLabel("On PCPartPicker, copy the text/Markdown part list and paste it here. "
                            "Direct URL fetching and automated scraping are not supported. "
                            "Imported items have unknown technical specs until you edit them.");
    info->setWordWrap(true);
    layout->addWidget(info);
    auto *text = new QPlainTextEdit;
    text->setPlaceholderText("CPU | [Processor Name](https://...) | $299.99\n"
                             "Motherboard | [Board Name](https://...) | $179.99");
    layout->addWidget(text);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    auto imported = importPCPartPickerText(text->toPlainText());
    if (imported.isEmpty()) {
        QMessageBox::warning(this, "Nothing imported",
                             "No recognized PCPartPicker rows found. Use the text part-list table.");
        return;
    }
    for (const auto &p : imported) {
        auto it = std::find_if(catalog_.parts.cbegin(), catalog_.parts.cend(), [&](const Part &existing) {
            return existing.category == p.category &&
                   existing.name.compare(p.name, Qt::CaseInsensitive) == 0;
        });
        if (it != catalog_.parts.cend()) build_.selected[p.category] = it->id;
        else {
            catalog_.parts.push_back(p);
            build_.selected[p.category] = p.id;
        }
    }
    saveCatalog();
    refreshCatalog();
    refreshBuild();
    message("Import complete", QString("Imported %1 table rows. Review prices and fill in "
                                      "missing specifications before relying on compatibility checks.")
                               .arg(imported.size()));
}
void MainWindow::generateOffline() {
    auto preferences = Preferences::fromPrompt(prompt_->toPlainText(), budget_->value());
    auto proposal = generateBuild(catalog_.parts, preferences);
    if (!proposal.ok()) {
        QMessageBox::warning(this, "Could not generate build", proposal.error);
        return;
    }
    build_ = proposal.build;
    refreshBuild();
    statusBar()->showMessage("Generated offline · " + displayMoney(proposal.total), 12000);
    message("Generated build", proposal.notes.join("\n"));
}
void MainWindow::generateWithCodex() {
    if (codex_ && codex_->state() != QProcess::NotRunning) return;
    QJsonArray available;
    for (const auto &p : catalog_.parts)
        available.append(p.toJson());
    const auto instructions = QString(
        "You are selecting parts for a PC builder. Return ONLY a valid JSON object "
        "with a key named selected, containing EXACTLY eight string keys: "
        "CPU, Motherboard, Memory, GPU, Storage, PSU, Case, Cooler. "
        "Each value MUST be an id from the supplied catalog in that category. "
        "Do not fabricate prices or parts. Respect compatibility and the budget. "
        "If no build exists, return JSON with an error string. "
        "User description: %1\nBudget USD: %2\nParts catalog JSON: %3")
        .arg(prompt_->toPlainText(),
             QString::number(Preferences::fromPrompt(prompt_->toPlainText(), budget_->value()).budget),
             QString::fromUtf8(QJsonDocument(available).toJson(QJsonDocument::Compact)));
    if (!codex_) {
        codex_ = new QProcess(this);
        connect(codex_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus status) { handleCodexOutput(code, status); });
        connect(codex_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            codexButton_->setEnabled(true);
            statusBar()->showMessage("Codex could not start. Install and authenticate the Codex CLI.", 12000);
        });
    }
    codexButton_->setEnabled(false);
    statusBar()->showMessage("Codex is selecting parts from your local catalog...");
#ifdef Q_OS_WIN
    codex_->setProgram("codex.cmd");
#else
    codex_->setProgram("codex");
#endif
    // Arguments are passed directly, never interpolated into a shell command.
    // Read-only sandbox: the AI cannot modify files through this integration.
    codex_->setArguments({"exec", "--skip-git-repo-check", "--sandbox", "read-only", instructions});
    codex_->start();
    const int runId = ++codexRun_;
    QTimer::singleShot(120000, codex_, [this, runId] {
        if (runId == codexRun_ && codex_ && codex_->state() != QProcess::NotRunning) {
            codex_->kill();
            statusBar()->showMessage("Codex timed out after 120 seconds.", 12000);
        }
    });
}
void MainWindow::handleCodexOutput(int exitCode, QProcess::ExitStatus status) {
    codexButton_->setEnabled(true);
    if (exitCode != 0 || status != QProcess::NormalExit) {
        QMessageBox::warning(this, "Codex failed",
                             "Codex did not complete successfully. Ensure the CLI is installed and logged in.\n" +
                             QString::fromUtf8(codex_->readAllStandardError()).left(600));
        return;
    }
    const QString raw = QString::fromUtf8(codex_->readAllStandardOutput());
    const int start = raw.indexOf('{'), end = raw.lastIndexOf('}');
    if (start < 0 || end <= start) {
        message("Invalid Codex reply", "The CLI did not return a JSON build. Try Generate Offline.");
        return;
    }
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(raw.mid(start, end - start + 1).toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        message("Invalid Codex reply", "The AI response could not be parsed as JSON.");
        return;
    }
    auto obj = doc.object();
    if (obj.contains("error")) {
        message("No suitable build", obj.value("error").toString());
        return;
    }
    const auto selected = obj.value("selected").toObject();
    Build proposed;
    proposed.name = "Codex-Recommended Build";
    for (const auto &category : categories()) {
        const auto id = selected.value(category).toString();
        const auto *part = catalog_.find(id);
        if (!part || part->category != category) {
            message("Invalid Codex selection", "Unrecognized or incorrect part for " + category +
                                              ". The current build was not changed.");
            return;
        }
        proposed.selected[category] = id;
    }
    auto check = validate(proposed, catalog_.parts, true);
    if (!check.compatible()) {
        message("Incompatible AI suggestion",
                "Codex proposed an incompatible build; it was rejected:\n" + check.errors.join("\n"));
        return;
    }
    const double budget = Preferences::fromPrompt(prompt_->toPlainText(), budget_->value()).budget;
    if (totalPrice(proposed, catalog_.parts) > budget + 0.01) {
        message("AI suggestion exceeds budget", "Codex returned a build above your budget; it was rejected.");
        return;
    }
    build_ = proposed;
    refreshBuild();
    statusBar()->showMessage("Codex build validated successfully.", 12000);
    message("Codex selection", "AI-selected component IDs were verified against your local catalog and "
                               "checked with PCForge's compatibility rules. Unknown specifications "
                               "still need manual verification.");
}
