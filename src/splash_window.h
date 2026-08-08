#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QTimer>
#include <QApplication>
#include <QScreen>

class SplashWindow : public QWidget {
public:
    explicit SplashWindow(const QString& title = "NSUM构建工具",
        QWidget* parent = nullptr)
        : QWidget(parent, Qt::SplashScreen | Qt::WindowStaysOnTopHint)
    {
        setWindowTitle(title);
        setFixedSize(420, 130);
        setWindowFlags(Qt::SplashScreen | Qt::WindowStaysOnTopHint);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(10);

        titleLabel_ = new QLabel(title, this);
        QFont titleFont = titleLabel_->font();
        titleFont.setPointSize(14);
        titleFont.setBold(true);
        titleLabel_->setFont(titleFont);

        statusLabel_ = new QLabel("正在初始化...", this);
        statusLabel_->setStyleSheet("color: #666;");

        progressBar_ = new QProgressBar(this);
        progressBar_->setRange(0, 0);
        progressBar_->setTextVisible(false);
        progressBar_->setFixedHeight(8);

        layout->addWidget(titleLabel_);
        layout->addWidget(statusLabel_);
        layout->addWidget(progressBar_);

        centerOnScreen();
    }

    void setStatus(const QString& text)
    {
        statusLabel_->setText(text);
        QApplication::processEvents();
    }

    void setProgress(int value, int maximum = 100)
    {
        if (maximum > 0 && value >= 0) {
            if (progressBar_->maximum() != maximum || progressBar_->minimum() != 0) {
                progressBar_->setRange(0, maximum);
                progressBar_->setTextVisible(true);
            }
            progressBar_->setValue(value);
        }
        QApplication::processEvents();
    }

    void setIndeterminate(bool on)
    {
        progressBar_->setRange(0, on ? 0 : 100);
        progressBar_->setTextVisible(!on);
        QApplication::processEvents();
    }

    void setTitle(const QString& text)
    {
        titleLabel_->setText(text);
        QApplication::processEvents();
    }

private:
    QLabel* titleLabel_;
    QLabel* statusLabel_;
    QProgressBar* progressBar_;

    void centerOnScreen()
    {
        if (auto* screen = QApplication::primaryScreen()) {
            QRect geo = screen->availableGeometry();
            move((geo.width() - width()) / 2,
                (geo.height() - height()) / 2);
        }
    }
};
