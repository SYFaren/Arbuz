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
    return I18n::t("ui.what_a_wonderful_syfaren");
}

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(I18n::t("ui.arbuz_first_run_setup"));
    setWindowIcon(ArbuzIcon::app());
    setWizardStyle(QWizard::ModernStyle);
    setPixmap(QWizard::LogoPixmap, QPixmap());
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setPixmap(QWizard::BannerPixmap, QPixmap());
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setButtonText(QWizard::FinishButton, I18n::t("ui.finish"));
    setButtonText(QWizard::CancelButton, I18n::t("ui.cancel"));

    auto *page = new QWizardPage(this);
    page->setTitle(I18n::t("ui.welcome_to_arbuz"));
    page->setSubTitle(I18n::t("ui.a_few_settings_then_you_can_calculate_in_the_grid"));

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
    form->addRow(I18n::t("ui.interface_language"), m_lang);
    connect(m_lang, &QComboBox::currentIndexChanged, this, [this](int) {
        I18n::setLang(m_lang->currentData().toString());
        if (m_joke)
            m_joke->setText(syfarenJoke());
        setWindowTitle(I18n::t("ui.arbuz_first_run_setup"));
        setButtonText(QWizard::FinishButton, I18n::t("ui.finish"));
        setButtonText(QWizard::CancelButton, I18n::t("ui.cancel"));
    });

    auto *pathRow = new QWidget(page);
    auto *pathLay = new QHBoxLayout(pathRow);
    pathLay->setContentsMargins(0, 0, 0, 0);
    m_path = new QLineEdit(AppSettings::instance().documentsPath(), pathRow);
    auto *browse = new QPushButton(I18n::t("ui.browse"), pathRow);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString d = QFileDialog::getExistingDirectory(this, I18n::t("ui.documents_folder"),
                                                            m_path->text());
        if (!d.isEmpty())
            m_path->setText(d);
    });
    pathLay->addWidget(m_path, 1);
    pathLay->addWidget(browse);
    form->addRow(I18n::t("ui.documents_folder"), pathRow);

    m_theme = new QComboBox(page);
    Theme::fillPresetCombo(m_theme, AppSettings::instance().themePreset());
    form->addRow(I18n::t("ui.theme"), m_theme);

    m_scale = new QSpinBox(page);
    m_scale->setRange(80, 160);
    m_scale->setSuffix(QStringLiteral(" %"));
    m_scale->setValue(AppSettings::instance().uiScalePercent());
    form->addRow(I18n::t("ui.ui_scale"), m_scale);

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
