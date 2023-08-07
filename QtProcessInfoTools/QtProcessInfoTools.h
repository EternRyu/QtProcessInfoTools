#pragma once
#ifndef QTPROCESSINFOTOOLS
#define QTPROCESSINFOTOOLS

#include <QtWidgets/QMainWindow>
#include "ui_QtProcessInfoTools.h"
#include <Windows.h>
#include <iostream>

class QtProcessInfoTools : public QMainWindow
{
    Q_OBJECT

public:
    QtProcessInfoTools(QWidget *parent = nullptr);
    ~QtProcessInfoTools();
public:
    
    VOID InitControl();
    VOID DriverToRun();
    VOID DriverToStop();
    VOID InsertInfo();
private:
    Ui::QtProcessInfoToolsClass ui;
};

#endif // QTPROCESSINFOTOOLS