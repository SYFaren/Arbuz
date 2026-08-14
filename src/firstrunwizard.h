#ifndef ARBUZ_FIRSTRUNWIZARD_H
#define ARBUZ_FIRSTRUNWIZARD_H

#include <QWizard>

class QLabel;
class QComboBox;
class QLineEdit;
class QSpinBox;

class FirstRunWizard : public QWizard
{
    Q_OBJECT
public:
    explicit FirstRunWizard(QWidget *parent = nullptr);

private:
    void applySettings();

    QLabel *m_joke = nullptr;
    QComboBox *m_lang = nullptr;
    QLineEdit *m_path = nullptr;
    QComboBox *m_theme = nullptr;
    QSpinBox *m_scale = nullptr;
};

#endif
