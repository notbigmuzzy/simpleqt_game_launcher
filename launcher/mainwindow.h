#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QScrollArea>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct GameInfo {
    QString name;
    QString exec;
    QString icon;
    QGroupBox *groupBox;
    QLabel *iconLabel;
    QPushButton *button;
    QProcess *process;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void launchGame();

private:
    void setupGamesFromCSV();
    void createGameWidget(const GameInfo &game, int row, int col);
    QString getGameNameFromDesktop(const QString &desktopFile);
    QColor getAverageColor(const QPixmap &pixmap);
    
    Ui::MainWindow *ui;
    QGridLayout *gamesLayout;
    QMap<QString, GameInfo> games;
};
#endif // MAINWINDOW_H
