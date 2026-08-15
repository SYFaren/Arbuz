#include "settingsdialog.h"
#include "appsettings.h"
#include "arbuzicon.h"
#include "i18n.h"
#include "theme.h"

#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::t("ui.settings_2"));
    setWindowIcon(ArbuzIcon::app());
    resize(540, 600);

    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setVerticalSpacing(10);
    form->setHorizontalSpacing(12);

    m_lang = new QComboBox(this);
    {
        const QString code = I18n::lang();
        int cur = 0;
        for (const auto &L : I18n::availableLanguages()) {
            m_lang->addItem(L.name, L.code);
            if (L.code == code)
                cur = m_lang->count() - 1;
        }
        m_lang->setCurrentIndex(cur);
    }
    form->addRow(I18n::t("ui.language"), m_lang);

    auto *pathRow = new QWidget(this);
    auto *pathLay = new QHBoxLayout(pathRow);
    pathLay->setContentsMargins(0, 0, 0, 0);
    m_path = new QLineEdit(AppSettings::instance().documentsPath(), pathRow);
    auto *browse = new QPushButton(I18n::t("ui.browse"), pathRow);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString d = QFileDialog::getExistingDirectory(this, QString(), m_path->text());
        if (!d.isEmpty())
            m_path->setText(d);
    });
    pathLay->addWidget(m_path, 1);
    pathLay->addWidget(browse);
    form->addRow(I18n::t("ui.documents_folder"), pathRow);

    m_scale = new QSpinBox(this);
    m_scale->setRange(80, 160);
    m_scale->setSuffix(QStringLiteral(" %"));
    m_scale->setValue(AppSettings::instance().uiScalePercent());
    form->addRow(I18n::t("ui.scale"), m_scale);

    auto *themeRow = new QWidget(this);
    auto *themeLay = new QHBoxLayout(themeRow);
    themeLay->setContentsMargins(0, 0, 0, 0);
    m_preset = new QComboBox(themeRow);
    Theme::fillPresetCombo(m_preset, AppSettings::instance().themePreset());
    auto *saveAs = new QPushButton(I18n::t("ui.save_as_3"), themeRow);
    m_deleteTheme = new QPushButton(I18n::t("ui.delete"), themeRow);
    themeLay->addWidget(m_preset, 1);
    themeLay->addWidget(saveAs);
    themeLay->addWidget(m_deleteTheme);
    form->addRow(I18n::t("ui.theme"), themeRow);
    m_deleteTheme->setEnabled(!Theme::isBuiltin(m_preset->currentData().toString()));

    connect(m_preset, &QComboBox::currentIndexChanged, this, [this]() {
        applyCurrentTheme(true);
        refreshSwatches();
        m_deleteTheme->setEnabled(!Theme::isBuiltin(m_preset->currentData().toString()));
        emit themeChanged();
    });
    connect(saveAs, &QPushButton::clicked, this, &SettingsDialog::saveAsTheme);
    connect(m_deleteTheme, &QPushButton::clicked, this, &SettingsDialog::deleteTheme);
    lay->addLayout(form);

    auto *colorsHint = new QLabel(I18n::t("ui.colors_changes_apply_immediately_save_as_creates_your_th"),
                                 this);
    colorsHint->setWordWrap(true);
    lay->addWidget(colorsHint);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *colorsHost = new QWidget(scroll);
    auto *colors = new QFormLayout(colorsHost);
    colors->setContentsMargins(6, 6, 6, 12);
    colors->setVerticalSpacing(8);
    QStringList roles = Theme::colorRoles();
    roles.sort();
    for (const QString &role : roles) {
        auto *btn = new QPushButton(colorsHost);
        m_swatches.insert(role, btn);
        connect(btn, &QPushButton::clicked, this, [this, role]() { pickColor(role); });
        colors->addRow(Theme::roleTitle(role), btn);
    }
    refreshSwatches();
    scroll->setWidget(colorsHost);
    lay->addWidget(scroll, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *ok = box->button(QDialogButtonBox::Ok))
        ok->setText(I18n::t("ui.ok"));
    if (QPushButton *cancel = box->button(QDialogButtonBox::Cancel))
        cancel->setText(I18n::t("ui.cancel"));
    connect(box, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);
}

void SettingsDialog::refreshSwatches()
{
    const auto pal = Theme::currentPalette();
    for (auto it = m_swatches.begin(); it != m_swatches.end(); ++it) {
        const QColor c = pal.value(it.key());
        it.value()->setText(c.name(QColor::HexRgb));
        it.value()->setStyleSheet(QStringLiteral("background:%1; color:%2;")
                                      .arg(c.name(QColor::HexRgb),
                                           c.lightness() < 128 ? QStringLiteral("#FFFFFF")
                                                               : QStringLiteral("#111111")));
    }
}

QHash<QString, QColor> SettingsDialog::swatchPalette() const
{
    QHash<QString, QColor> pal;
    for (auto it = m_swatches.cbegin(); it != m_swatches.cend(); ++it)
        pal.insert(it.key(), QColor(it.value()->text()));
    return pal;
}

void SettingsDialog::applyCurrentTheme(bool clearOverrides)
{
    const QString id = m_preset->currentData().toString();
    if (clearOverrides)
        Theme::applyPreset(id);
    else {
        AppSettings::instance().setThemePreset(id);
        Theme::apply(qApp);
    }
}

void SettingsDialog::pickColor(const QString &role)
{
    QPushButton *btn = m_swatches.value(role);
    if (!btn)
        return;
    const QColor c = QColorDialog::getColor(QColor(btn->text()), this, Theme::roleTitle(role));
    if (!c.isValid())
        return;
    btn->setText(c.name(QColor::HexRgb));
    btn->setStyleSheet(QStringLiteral("background:%1; color:%2;")
                           .arg(c.name(QColor::HexRgb),
                                c.lightness() < 128 ? QStringLiteral("#FFFFFF")
                                                    : QStringLiteral("#111111")));
    AppSettings::instance().setColorOverride(role, c);
    Theme::apply(qApp);
    emit themeChanged();
}

void SettingsDialog::saveAsTheme()
{
    bool ok = false;
    const QString title = QInputDialog::getText(
        this, I18n::t("ui.new_theme"),
        I18n::t("ui.name_2"), QLineEdit::Normal,
        I18n::t("ui.my_theme"), &ok);
    if (!ok || title.trimmed().isEmpty())
        return;
    const QString id = Theme::saveUserTheme(title.trimmed(), swatchPalette());
    if (id.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Arbuz"),
                             I18n::t("ui.could_not_save_the_theme"));
        return;
    }
    Theme::applyPreset(id);
    Theme::fillPresetCombo(m_preset, id);
    m_deleteTheme->setEnabled(true);
    refreshSwatches();
    emit themeChanged();
}

void SettingsDialog::deleteTheme()
{
    const QString id = m_preset->currentData().toString();
    if (Theme::isBuiltin(id))
        return;
    if (QMessageBox::question(this, QStringLiteral("Arbuz"),
                              I18n::t("ui.delete_theme_1").arg(Theme::presetTitle(id)))
        != QMessageBox::Yes)
        return;
    Theme::deleteUserTheme(id);
    Theme::applyPreset(QStringLiteral("white"));
    Theme::fillPresetCombo(m_preset, QStringLiteral("white"));
    m_deleteTheme->setEnabled(false);
    refreshSwatches();
    emit themeChanged();
}

void SettingsDialog::save()
{
    const QString oldLang = I18n::lang();
    const QString newLang = m_lang->currentData().toString();
    AppSettings::instance().setLanguage(newLang);
    AppSettings::instance().setDocumentsPath(m_path->text());
    AppSettings::instance().setUiScalePercent(m_scale->value());
    AppSettings::instance().setThemePreset(m_preset->currentData().toString());
    Theme::apply(qApp);
    emit themeChanged();
    if (oldLang != newLang) {
        QMessageBox::information(
            this, QStringLiteral("Arbuz"),
            I18n::t("ui.menu_language_will_apply_after_you_restart_arbuz"));
    }
    accept();
}
