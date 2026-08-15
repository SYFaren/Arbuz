#include "functiondialog.h"
#include "arbuzicon.h"
#include "formulaengine.h"
#include "i18n.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

static QString categoryTitle(const QString &id)
{
    return I18n::t(QStringLiteral("fn.cat.") + id);
}

FunctionBrowser::FunctionBrowser(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("functionSearch"));
    m_search->setPlaceholderText(I18n::t("ui.search_function"));
    lay->addWidget(m_search);

    m_cat = new QComboBox(this);
    m_cat->setObjectName(QStringLiteral("functionCategory"));
    m_cat->addItem(categoryTitle(QStringLiteral("all")), QStringLiteral("all"));
    for (const QString &c : FormulaEngine::categories())
        m_cat->addItem(categoryTitle(c), c);
    lay->addWidget(m_cat);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("functionList"));
    m_list->setUniformItemSizes(true);
    lay->addWidget(m_list, 1);

    m_syntax = new QLabel(this);
    m_syntax->setObjectName(QStringLiteral("functionSyntax"));
    QFont mono = font();
    mono.setFamily(QStringLiteral("monospace"));
    m_syntax->setFont(mono);
    m_syntax->setWordWrap(true);
    m_syntax->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_syntax->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_syntax);

    m_help = new QLabel(this);
    m_help->setObjectName(QStringLiteral("functionHelp"));
    m_help->setWordWrap(true);
    m_help->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_help->setMinimumHeight(m_help->fontMetrics().height() * 2 + 8);
    m_help->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_help);

    connect(m_cat, &QComboBox::currentIndexChanged, this, [this](int) { rebuild(); });
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &) { rebuild(); });
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { updateDescription(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        emit functionChosen(selectedName());
    });
    rebuild();
}

QString FunctionBrowser::selectedName() const
{
    const QListWidgetItem *item = m_list ? m_list->currentItem() : nullptr;
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void FunctionBrowser::setFilterText(const QString &text)
{
    if (m_search && m_search->text() != text)
        m_search->setText(text);
}

void FunctionBrowser::rebuild()
{
    const QString cat = m_cat ? m_cat->currentData().toString() : QStringLiteral("all");
    const QString q = m_search ? m_search->text().trimmed() : QString();
    const QString keep = selectedName();
    m_list->clear();
    for (const auto &fn : FormulaEngine::catalog()) {
        if (cat != QLatin1String("all") && fn.category != cat)
            continue;
        if (!q.isEmpty() && !fn.name.contains(q, Qt::CaseInsensitive)
            && !fn.helpRu().contains(q, Qt::CaseInsensitive)
            && !fn.helpEn().contains(q, Qt::CaseInsensitive))
            continue;
        auto *item = new QListWidgetItem(fn.name, m_list);
        item->setData(Qt::UserRole, fn.name);
        item->setToolTip(fn.help());
    }
    if (m_list->count() == 0) {
        updateDescription();
        return;
    }
    int row = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == keep) {
            row = i;
            break;
        }
    }
    m_list->setCurrentRow(row);
    updateDescription();
}

void FunctionBrowser::updateDescription()
{
    const QString name = selectedName();
    for (const auto &fn : FormulaEngine::catalog()) {
        if (fn.name != name)
            continue;
        m_syntax->setText(fn.syntax);
        m_help->setText(fn.help());
        return;
    }
    m_syntax->clear();
    m_help->clear();
}

FunctionDialog::FunctionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::t("ui.function_2"));
    setWindowIcon(ArbuzIcon::app());
    setObjectName(QStringLiteral("functionDialog"));
    resize(440, 520);

    auto *lay = new QVBoxLayout(this);
    auto *hint = new QLabel(I18n::t("ui.select_a_function_and_click_insert_double_click_also_ins"),
                            this);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    m_browser = new FunctionBrowser(this);
    lay->addWidget(m_browser, 1);
    connect(m_browser, &FunctionBrowser::functionChosen, this, &QDialog::accept);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(I18n::t("ui.insert_2"));
    if (QPushButton *cancel = box->button(QDialogButtonBox::Cancel))
        cancel->setText(I18n::t("ui.cancel"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);
}

QString FunctionDialog::selectedName() const
{
    return m_browser ? m_browser->selectedName() : QString();
}
