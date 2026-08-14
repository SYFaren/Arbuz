#ifndef ARBUZ_FUNCTIONDIALOG_H
#define ARBUZ_FUNCTIONDIALOG_H

#include <QDialog>
#include <QWidget>

class QListWidget;
class QLabel;
class QComboBox;
class QLineEdit;

class FunctionBrowser : public QWidget
{
    Q_OBJECT
public:
    explicit FunctionBrowser(QWidget *parent = nullptr);
    QString selectedName() const;
    void setFilterText(const QString &text);

signals:
    void functionChosen(const QString &name);

private:
    void rebuild();
    void updateDescription();

    QComboBox *m_cat = nullptr;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_syntax = nullptr;
    QLabel *m_help = nullptr;
};

class FunctionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FunctionDialog(QWidget *parent = nullptr);
    QString selectedName() const;

private:
    FunctionBrowser *m_browser = nullptr;
};

#endif
