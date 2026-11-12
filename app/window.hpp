#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

class Window : public QWidget {
    QProcess host_, driver_;
    QSettings settings_;
    QLabel *status_, *detail_, *mapping_;
    QPushButton *start_, *load_;
    QSpinBox *fps_;
    QCheckBox *input_;
    QComboBox *output_;
    QPlainTextEdit *logs_;
    QTimer refresh_;
    QString pending_, desiredOutput_, hostPath_;
    QSet<QString> mapped_;
    bool closing_ = false, stopping_ = false;
    void start();
    void stop();
    void refresh();
    void mapInput();
    void consumeOutput();
    void setStatus(const QString &title, const QString &detail);
    void appendLog(const QString &line);
    void loadDriver();
    QStringList cards() const;
    QString virtualOutput() const;
    void updateScreens();

  protected:
    void closeEvent(QCloseEvent *event) override;

  public:
    explicit Window();
    void startWhenReady();
};
