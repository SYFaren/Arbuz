#include "firstrunwizard.h"
#include "appsettings.h"
#include "arbuzicon.h"
#include "i18n.h"
#include "theme.h"

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWizardPage>

static QString syfarenJoke()
{
    return QStringLiteral("Какой же SYFaren прекрасный");
}

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(I18n::t("Первичная настройка Arbuz", "Arbuz first-run setup"));
    setWindowIcon(ArbuzIcon::app());
    setWizardStyle(QWizard::ModernStyle);
    setPixmap(QWizard::LogoPixmap, QPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setPixmap(QWizard::BannerPixmap, QPixmap());
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setButtonText(QWizard::FinishButton, I18n::t("Готово", "Finish"));
    setButtonText(QWizard::CancelButton, I18n::t("Отмена", "Cancel"));

    auto *page = new QWizardPage(this);
    page->setTitle(I18n::t("Добро пожаловать в Arbuz", "Welcome to Arbuz"));
    page->setSubTitle(I18n::t("Несколько настроек — и можно считать в сетке.",
                              "A few settings, then you can calculate in the grid."));

    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(8);

    m_joke = new QLabel(syfarenJoke(), page);
    m_joke->setObjectName(QStringLiteral("syfarenJoke"));
    m_joke->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_joke->setWordWrap(true);
    root->addWidget(m_joke);

    auto *form = new QFormLayout();
    form->setVerticalSpacing(10);
    form->setHorizontalSpacing(12);
    form->setContentsMargins(0, 4, 0, 0);
    m_lang = new QComboBox(page);
    m_lang->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    m_lang->addItem(QStringLiteral("English"), QStringLiteral("en"));
    if (I18n::lang() == QLatin1String("en"))
        m_lang->setCurrentIndex(1);
    form->addRow(I18n::t("Язык интерфейса", "Interface language"), m_lang);

    auto *pathRow = new QWidget(page);
    auto *pathLay = new QHBoxLayout(pathRow);
    pathLay->setContentsMargins(0, 0, 0, 0);
    m_path = new QLineEdit(AppSettings::instance().documentsPath(), pathRow);
    auto *browse = new QPushButton(I18n::t("Обзор…", "Browse…"), pathRow);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString d = QFileDialog::getExistingDirectory(this, I18n::t("Папка для файлов", "Documents folder"),
                                                            m_path->text());
        if (!d.isEmpty())
            m_path->setText(d);
    });
    pathLay->addWidget(m_path, 1);
    pathLay->addWidget(browse);
    form->addRow(I18n::t("Папка для файлов", "Documents folder"), pathRow);

    m_theme = new QComboBox(page);
    Theme::fillPresetCombo(m_theme, AppSettings::instance().themePreset());
    form->addRow(I18n::t("Тема", "Theme"), m_theme);

    m_scale = new QSpinBox(page);
    m_scale->setRange(80, 160);
    m_scale->setSuffix(QStringLiteral(" %"));
    m_scale->setValue(AppSettings::instance().uiScalePercent());
    form->addRow(I18n::t("Масштаб интерфейса", "UI scale"), m_scale);

    root->addLayout(form);
    root->addStretch(1);

    addPage(page);

    connect(this, &QDialog::accepted, this, &FirstRunWizard::applySettings);
}

void FirstRunWizard::applySettings()
{
    AppSettings::instance().setLanguage(m_lang->currentData().toString());
    AppSettings::instance().setDocumentsPath(m_path->text());
    AppSettings::instance().setUiScalePercent(m_scale->value());
    Theme::applyPreset(m_theme->currentData().toString());
    AppSettings::instance().setFirstRunDone(true);
}
