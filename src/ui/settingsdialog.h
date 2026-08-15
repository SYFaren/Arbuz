#ifndef ARBUZ_SETTINGSDIALOG_H
#define ARBUZ_SETTINGSDIALOG_H

#include <QColor>
#include <QDialog>
#include <QHash>
#include <QPushButton>

class QComboBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

signals:
    void themeChanged();

private:
    void pickColor(const QString &role);
    void refreshSwatches();
    QHash<QString, QColor> swatchPalette() const;
    void applyCurrentTheme(bool clearOverrides);
    void saveAsTheme();
    void deleteTheme();
    void save();

    QComboBox *m_lang = nullptr;
    QLineEdit *m_path = nullptr;
    QComboBox *m_preset = nullptr;
    QSpinBox *m_scale = nullptr;
    QPushButton *m_deleteTheme = nullptr;
    QHash<QString, QPushButton *> m_swatches;
};

#endif
