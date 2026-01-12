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
#include <QTime>
#include <cstdlib>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Seed random number generator
    srand(QTime::currentTime().msec());
    
    // Set window flags to remove maximize button and make window non-resizable
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    setFixedSize(size());
    
    // Set application icon
    QIcon appIcon;
    // Try to use a gaming-related icon
    appIcon = QIcon::fromTheme("applications-games");
    if (appIcon.isNull()) {
        // Fallback to other game-related icons
        QStringList iconNames = {"games", "game", "joystick", "gamepad", "applications-other"};
        for (const QString &iconName : iconNames) {
            appIcon = QIcon::fromTheme(iconName);
            if (!appIcon.isNull()) break;
        }
    }
    if (!appIcon.isNull()) {
        setWindowIcon(appIcon);
    }
    
    // Set main window background to semi-transparent black
    setStyleSheet(
        "QMainWindow { background-color: rgba(0, 0, 0, 200); }"
        "QWidget#centralwidget { background-color: transparent; }"
        "QScrollArea { background-color: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background-color: transparent; }"
        "QScrollBar:vertical {"
        "    background-color: rgba(40, 40, 40, 150);"
        "    width: 5px;"
        "    border: none;"
        "    border-radius: 2px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: rgba(100, 100, 100, 180);"
        "    border-radius: 2px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background-color: rgba(150, 150, 150, 200);"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    border: none;"
        "    background: none;"
        "}"
    );
    setAttribute(Qt::WA_TranslucentBackground);
    
    // Setup the games layout
    gamesLayout = new QGridLayout();
    gamesLayout->setSpacing(10);
    ui->scrollAreaWidgetContents->setLayout(gamesLayout);
    
    // Connect search functionality
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::filterGames);
    
    // Set default status bar message
    ui->statusbar->showMessage("Pick a game and have fun ...");
    
    // Initialize game count label and add to status bar
    gameCountLabel = new QLabel(this);
    gameCountLabel->setStyleSheet("QLabel { color: white; font-size: 12px; }");
    ui->statusbar->addPermanentWidget(gameCountLabel);
    
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
    QFile file("/home/nbm/Code/simpleqt_game_launcher/games_exec_icon.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Could not open games_exec_icon.csv file");
        return;
    }
    
    QTextStream in(&file);
    QString line = in.readLine(); // Skip header
    
    // Games to skip
    QStringList skipGames = {"asciijump.desktop", "blastem.desktop", "dreamchess.desktop", "dsda-doom.desktop"};
    
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
        
        createGameWidget(game, 0, 0); // Position doesn't matter here, will be set in updateGameLayout
        games[game.name] = game;
    }
    
    file.close();
    
    // Initial layout of all games
    updateGameLayout();
    
    // Set random status bar color from game colors
    setRandomStatusBarColor();
    
    // Update game count display
    updateGameCount();
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
    
    // Store the color for potential status bar use
    gameColors.append(backgroundColor);
    
    // Apply background color to group box
    QString styleSheet = QString(
        "QGroupBox {"
        "    background-color: rgba(%1, %2, %3, 180);"
        "    border: 2px solid rgba(%1, %2, %3, 255);"
        "    border-radius: 8px;"
        "    font-weight: bold;"
        "    padding-top: 18px;"
        "    text-align: center;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    subcontrol-position: top center;"
        "    top: 5px;"
        "    padding: 2px 10px 2px 5px;"
        "    color: white;"
        "    border-radius: 3px;"
        "    border: 0px;"
        "}"
    ).arg(backgroundColor.red())
     .arg(backgroundColor.green())
     .arg(backgroundColor.blue());
    
    groupBox->setStyleSheet(styleSheet);
    
    // Create launch button
    QPushButton *button = new QPushButton("Launch");
    button->setFixedSize(100, 30);
    button->setProperty("gameName", game.name);
    
    // Style the button with black background and white text
    button->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(255, 255, 255, 30);"
        "    border: none;"
        "    color: white;"
        "    border-radius: 4px;"
        "    padding: 2px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(255, 255, 255, 80);"
        "    border: none;"
        "    color: white;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(255, 255, 255, 50);"
        "    border: none;"
        "    color: white;"
        "}"
    );
    
    // Connect button to launch slot
    connect(button, &QPushButton::clicked, this, &MainWindow::launchGame);
    
    // Layout inside group box
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(iconLabel, 0, Qt::AlignHCenter);
    layout->addWidget(button, 0, Qt::AlignHCenter);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(10);
    groupBox->setLayout(layout);
    
    // Don't add to main grid layout here - will be done in updateGameLayout
    // Initially hide the widget
    groupBox->setParent(ui->scrollAreaWidgetContents);
    groupBox->hide();
    
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
                    // Reset status bar message when game closes
                    ui->statusbar->showMessage("Pick a game and have fun ...");
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
        // Update status bar message
        ui->statusbar->showMessage(QString("Game %1 is running ...").arg(gameName));
        showMinimized();
    }
}

void MainWindow::filterGames()
{
    updateGameLayout();
}

void MainWindow::updateGameLayout()
{
    // Clear current layout
    QLayoutItem *item;
    while ((item = gamesLayout->takeAt(0))) {
        if (item->widget()) {
            item->widget()->hide();
        }
        delete item;
    }
    
    QString searchTerm = ui->searchLineEdit->text().toLower();
    int row = 0, col = 0;
    const int maxCols = 5;
    
    // Show games that match search term
    for (auto it = games.begin(); it != games.end(); ++it) {
        GameInfo &game = it.value();
        
        if (searchTerm.isEmpty() || game.name.toLower().contains(searchTerm)) {
            gamesLayout->addWidget(game.groupBox, row, col);
            game.groupBox->show();
            
            col++;
            if (col >= maxCols) {
                col = 0;
                row++;
            }
        } else {
            game.groupBox->hide();
        }
    }
    
    // Update game count after layout change
    updateGameCount();
}

void MainWindow::setRandomStatusBarColor()
{
    if (gameColors.isEmpty()) {
        return; // No colors available, keep default
    }
    
    // Pick a random color from the game colors
    int randomIndex = rand() % gameColors.size();
    QColor selectedColor = gameColors[randomIndex];
    
    // Increase color intensity by 25%
    int h, s, v;
    selectedColor.getHsv(&h, &s, &v);
    
    // Increase saturation by 25% (clamped to 255)
    s = qMin(255, static_cast<int>(s * 1.25));
    
    // Also slightly increase value/brightness by 10% for more intensity
    v = qMin(255, static_cast<int>(v * 1.10));
    
    QColor intensifiedColor;
    intensifiedColor.setHsv(h, s, v);
    
    // Apply the intensified color to status bar
    QString statusBarStyle = QString(
        "QStatusBar {"
        "    background-color: rgba(%1, %2, %3, 200);"
        "    color: white;"
        "    border-top: 1px solid rgba(%1, %2, %3, 255);"
        "    font-size: 12px;"
        "}"
    ).arg(intensifiedColor.red())
     .arg(intensifiedColor.green())
     .arg(intensifiedColor.blue());
    
    ui->statusbar->setStyleSheet(statusBarStyle);
}

void MainWindow::updateGameCount()
{
    int visibleCount = 0;
    int totalCount = games.size();
    
    // Count visible games
    for (const auto &game : games) {
        if (game.groupBox && game.groupBox->isVisible()) {
            visibleCount++;
        }
    }
    
    // Update the label text
    if (visibleCount == totalCount || visibleCount == 0) {
        gameCountLabel->setText(QString("Number of Games: %1").arg(totalCount));
    } else {
        gameCountLabel->setText(QString("Games: %1 / %2").arg(visibleCount).arg(totalCount));
    }
}