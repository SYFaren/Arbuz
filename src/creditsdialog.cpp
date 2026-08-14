#include "creditsdialog.h"
#include "arbuzicon.h"
#include "i18n.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

CreditsDialog::CreditsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::t("Благодарности", "Credits"));
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
    auto *head = new QLabel(I18n::t("Arbuz — создатель SYFaren", "Arbuz — created by SYFaren"), this);
    QFont f = head->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 3);
    head->setFont(f);
    titles->addWidget(head);

    auto *sub = new QLabel(
        I18n::t("Открытые проекты, без которых этой программы не было бы.",
                "Open projects without which this program would not exist."),
        this);
    sub->setWordWrap(true);
    titles->addWidget(sub);
    titleRow->addLayout(titles, 1);
    lay->addLayout(titleRow);

    auto *text = new QTextBrowser(this);
    text->setObjectName(QStringLiteral("creditsView"));
    text->setOpenExternalLinks(true);
    text->setOpenLinks(true);
    QFile file(QStringLiteral(":/arbuz/CREDITS.md"));
    QString md;
    if (file.open(QIODevice::ReadOnly))
        md = QString::fromUtf8(file.readAll());
    text->setMarkdown(md);
    lay->addWidget(text, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    if (QPushButton *ok = box->button(QDialogButtonBox::Ok))
        ok->setText(I18n::t("Закрыть", "Close"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    lay->addWidget(box);
}
