#include "creditsdialog.h"
#include "arbuzicon.h"
#include "i18n.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextDocument>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

CreditsDialog::CreditsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::t("ui.credits"));
    setWindowIcon(ArbuzIcon::app());
    resize(680, 520);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 12, 12, 12);

    auto *titleRow = new QHBoxLayout();
    auto *iconLbl = new QLabel(this);
    iconLbl->setPixmap(ArbuzIcon::app().pixmap(48, 48));
    iconLbl->setFixedSize(48, 48);
    titleRow->addWidget(iconLbl, 0, Qt::AlignTop);

    auto *titles = new QVBoxLayout();
    auto *head = new QLabel(I18n::t("ui.arbuz_table_calculator_syfaren"),
                           this);
    QFont f = head->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 3);
    head->setFont(f);
    head->setWordWrap(true);
    titles->addWidget(head);

    auto *sub = new QLabel(
        I18n::t("ui.open_projects_without_which_this_program_would_not_exist"),
        this);
    sub->setWordWrap(true);
    titles->addWidget(sub);
    titleRow->addLayout(titles, 1);
    lay->addLayout(titleRow);

    auto *text = new QTextBrowser(this);
    text->setObjectName(QStringLiteral("creditsView"));
    text->setOpenExternalLinks(true);
    text->setOpenLinks(true);
    text->document()->setDocumentMargin(12);
    text->setMarkdown(I18n::creditsMarkdown());
    lay->addWidget(text, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    if (QPushButton *ok = box->button(QDialogButtonBox::Ok))
        ok->setText(I18n::t("ui.close"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    lay->addWidget(box);
}
