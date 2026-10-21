#include "core.h"
#include "optimizer.h"

#include <QTemporaryDir>
#include <QFile>
#include <QtTest>

using namespace pcforge;

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void catalogRoundTrip() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Catalog c;
        QString error;
        QVERIFY2(c.load(dir.path() + "/parts.json", &error), qPrintable(error));
        QCOMPARE(c.parts.size(), 20);
        Catalog loaded;
        QVERIFY(loaded.load(dir.path() + "/parts.json", &error));
        QCOMPARE(loaded.parts.size(), c.parts.size());
        QCOMPARE(loaded.parts.at(0).name, c.parts.at(0).name);
    }

    void invalidCatalogIsNotSilentlyReplaced() {
        QTemporaryDir dir;
        const auto path = dir.path() + "/parts.json";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not valid json");
        file.close();
        Catalog c;
        QString error;
        QVERIFY(!c.load(path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("not valid json"));
    }

    void invalidSocketDetected() {
        Catalog c;
        c.seedExamples();
        Build b;
        for (const auto &p : c.parts) {
            if (p.category == "CPU" && p.specs.value("socket").toString() == "AM5") b.selected["CPU"] = p.id;
            if (p.category == "Motherboard" && p.specs.value("socket").toString() == "LGA1700")
                b.selected["Motherboard"] = p.id;
        }
        auto v = validate(b, c.parts);
        QVERIFY(!v.compatible());
        QVERIFY(v.errors.join(" ").contains("socket"));
    }

    void clearanceAndPowerDetected() {
        Catalog c;
        c.seedExamples();
        Build b;
        for (const auto &p : c.parts) {
            if (p.category == "CPU" && p.specs.value("power_w").toInt() == 170) b.selected["CPU"] = p.id;
            if (p.category == "GPU" && p.specs.value("gpu_length_mm").toInt() == 340) b.selected["GPU"] = p.id;
            if (p.category == "PSU" && p.specs.value("capacity_w").toInt() == 750) b.selected["PSU"] = p.id;
            if (p.category == "Case" && p.specs.value("max_gpu_length_mm").toInt() == 330) b.selected["Case"] = p.id;
        }
        const auto v = validate(b, c.parts);
        QVERIFY(!v.compatible());
        const auto errors = v.errors.join(" ");
        QVERIFY(errors.contains("too long"));
        QVERIFY(errors.contains("PSU"));
    }

    void optimizerGeneratesCompatibleBuild() {
        Catalog c;
        c.seedExamples();
        auto proposal = generateBuild(c.parts, Preferences::fromPrompt("gaming $2200"));
        QVERIFY2(proposal.ok(), qPrintable(proposal.error));
        QCOMPARE(proposal.build.selected.size(), 8);
        QVERIFY(validate(proposal.build, c.parts, true).compatible());
        QVERIFY(proposal.total <= 2200.0);
    }

    void optimizerRejectsImpossibleBudget() {
        Catalog c;
        c.seedExamples();
        auto proposal = generateBuild(c.parts, Preferences::fromPrompt("gaming $100"));
        QVERIFY(!proposal.ok());
        QVERIFY(!proposal.error.isEmpty());
    }

    void parserReadsPartPickerTable() {
        auto imported = importPCPartPickerText(
            "CPU | [Processor Model](https://example.com/part) | $329.99\n"
            "Video Card | [Graphics Model](https://example.com/card) | $699.00\n"
            "Type | Item | Price\n");
        QCOMPARE(imported.size(), 2);
        QCOMPARE(imported[0].category, QString("CPU"));
        QCOMPARE(imported[0].name, QString("Processor Model"));
        QCOMPARE(imported[0].price, 329.99);
        QCOMPARE(imported[1].category, QString("GPU"));
        QCOMPARE(imported[1].price, 699.00);
        QVERIFY(imported[0].specs.isEmpty());
    }

    void missingSpecsReported() {
        Catalog c;
        c.seedExamples();
        Build b;
        auto imported = importPCPartPickerText("CPU | Plain CPU | $200\nMotherboard | Board | $100");
        QVERIFY(imported.size() == 2);
        for (const auto &p : imported) {
            c.parts.push_back(p);
            b.selected[p.category] = p.id;
        }
        auto v = validate(b, c.parts);
        QVERIFY(v.compatible());
        QVERIFY(!v.warnings.isEmpty());
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "test_core.moc"
