#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QMessageBox>
#include <QIcon>
#include <QPixmap>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColor>
#include <QImage>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Set window flags to remove maximize button and make window non-resizable
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setFixedSize(size());
    
    // Setup the games layout
    gamesLayout = new QGridLayout();
    gamesLayout->setSpacing(10);
    ui->scrollAreaWidgetContents->setLayout(gamesLayout);
    
    // Load and setup games from CSV
    setupGamesFromCSV();
}

MainWindow::~MainWindow()
{
    // Clean up all running processes
    for (auto &game : games) {
        if (game.process && game.process->state() != QProcess::NotRunning) {
            game.process->kill();
            game.process->waitForFinished(3000);
        }
    }
    delete ui;
}

void MainWindow::setupGamesFromCSV()
{
    QFile file("/home/Commodore/Code/launcher/games_exec_icon.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Could not open games_exec_icon.csv file");
        return;
    }
    
    QTextStream in(&file);
    QString line = in.readLine(); // Skip header
    
    // Games to skip
    QStringList skipGames = {"asciijump.desktop", "blastem.desktop"};
    
    int row = 0, col = 0;
    const int maxCols = 5; // 5 games per row
    
    while (!in.atEnd()) {
        line = in.readLine();
        if (line.isEmpty()) continue;
        
        // Parse CSV line (remove quotes and split by comma)
        QStringList parts = line.split("\",\"");
        if (parts.size() < 3) continue;
        
        QString desktopFile = parts[0].mid(1); // Remove leading quote
        QString exec = parts[1];
        QString icon = parts[2];
        icon = icon.left(icon.length() - 1); // Remove trailing quote
        
        // Skip unwanted games
        if (skipGames.contains(desktopFile)) {
            continue;
        }
        
        GameInfo game;
        game.name = getGameNameFromDesktop(desktopFile);
        game.exec = exec;
        game.icon = icon;
        game.process = nullptr;
        
        createGameWidget(game, row, col);
        games[game.name] = game;
        
        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
    
    file.close();
}

QString MainWindow::getGameNameFromDesktop(const QString &desktopFile)
{
    QString name = desktopFile;
    name = name.replace(".desktop", "");
    name = name.replace("-", " ");
    name = name.replace("_", " ");
    
    // Capitalize first letter of each word
    QStringList words = name.split(" ");
    for (QString &word : words) {
        if (!word.isEmpty()) {
            word[0] = word[0].toUpper();
        }
    }
    return words.join(" ");
}

QColor MainWindow::getAverageColor(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        return QColor(64, 64, 64); // Default dark gray
    }
    
    QImage image = pixmap.toImage();
    if (image.isNull()) {
        return QColor(64, 64, 64);
    }
    
    // Scale down for performance
    if (image.width() > 64 || image.height() > 64) {
        image = image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    long long totalR = 0, totalG = 0, totalB = 0;
    int pixelCount = 0;
    
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QRgb pixel = image.pixel(x, y);
            int alpha = qAlpha(pixel);
            
            // Skip transparent pixels
            if (alpha > 128) {
                totalR += qRed(pixel);
                totalG += qGreen(pixel);
                totalB += qBlue(pixel);
                pixelCount++;
            }
        }
    }
    
    if (pixelCount == 0) {
        return QColor(64, 64, 64);
    }
    
    int avgR = totalR / pixelCount;
    int avgG = totalG / pixelCount;
    int avgB = totalB / pixelCount;
    
    // Make it darker and less saturated for better background, but with 20% more intensity
    avgR = avgR * 0.5 + 32;
    avgG = avgG * 0.5 + 32;
    avgB = avgB * 0.5 + 32;
    
    return QColor(avgR, avgG, avgB);
}

void MainWindow::createGameWidget(const GameInfo &game, int row, int col)
{
    // Create group box
    QGroupBox *groupBox = new QGroupBox(game.name);
    groupBox->setFixedSize(140, 160);
    
    // Create icon label
    QLabel *iconLabel = new QLabel();
    iconLabel->setFixedSize(80, 80);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setScaledContents(true);
    
    // Try to load icon
    QIcon gameIcon = QIcon::fromTheme(game.icon);
    if (gameIcon.isNull() && QFile::exists(game.icon)) {
        gameIcon = QIcon(game.icon);
    }
    
    QPixmap iconPixmap;
    QColor backgroundColor;
    
    if (!gameIcon.isNull()) {
        iconPixmap = gameIcon.pixmap(80, 80);
        iconLabel->setPixmap(iconPixmap);
        backgroundColor = getAverageColor(iconPixmap);
    } else {
        iconLabel->setText("🎮");
        iconLabel->setStyleSheet("font-size: 36px;");
        backgroundColor = QColor(64, 64, 64); // Default color for emoji fallback
    }
    
    // Apply background color to group box
    QString styleSheet = QString(
        "QGroupBox {"
        "    background-color: rgba(%1, %2, %3, 180);"
        "    border: 2px solid rgba(%1, %2, %3, 255);"
        "    border-radius: 8px;"
        "    font-weight: bold;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "    color: white;"
        "    background-color: rgba(%1, %2, %3, 255);"
        "    border-radius: 3px;"
        "}"
    ).arg(backgroundColor.red())
     .arg(backgroundColor.green())
     .arg(backgroundColor.blue());
    
    groupBox->setStyleSheet(styleSheet);
    
    // Create launch button
    QPushButton *button = new QPushButton("Launch");
    button->setFixedSize(100, 30);
    button->setProperty("gameName", game.name);
    
    // Connect button to launch slot
    connect(button, &QPushButton::clicked, this, &MainWindow::launchGame);
    
    // Layout inside group box
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(iconLabel, 0, Qt::AlignHCenter);
    layout->addWidget(button, 0, Qt::AlignHCenter);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(10);
    groupBox->setLayout(layout);
    
    // Add to main grid layout
    gamesLayout->addWidget(groupBox, row, col);
    
    // Store references in the game struct
    const_cast<GameInfo&>(game).groupBox = groupBox;
    const_cast<GameInfo&>(game).iconLabel = iconLabel;
    const_cast<GameInfo&>(game).button = button;
}

void MainWindow::launchGame()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QString gameName = button->property("gameName").toString();
    GameInfo &game = games[gameName];
    
    // If a process is already running, don't start another one
    if (game.process && game.process->state() != QProcess::NotRunning) {
        QMessageBox::information(this, "Info", QString("%1 is already running!").arg(gameName));
        return;
    }
    
    // Create process if needed
    if (!game.process) {
        game.process = new QProcess(this);
        
        // Connect to process finished signal
        connect(game.process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, gameName](int exitCode, QProcess::ExitStatus exitStatus) {
                    GameInfo &gameRef = games[gameName];
                    gameRef.button->setText("Launch");
                    // Restore the window when any game closes
                    showNormal();
                    raise();
                    activateWindow();
                });
    }
    
    // Parse exec command (handle parameters)
    QStringList execParts = game.exec.split(" ");
    QString program = execParts.takeFirst();
    
    // Start the game
    game.process->start(program, execParts);
    
    // Check if the process started successfully
    if (!game.process->waitForStarted(3000)) {
        QMessageBox::critical(this, "Error", 
                             QString("Failed to start %1: %2").arg(gameName).arg(game.process->errorString()));
    } else {
        // Change button text and minimize window
        game.button->setText("Running..");
        showMinimized();
    }
}