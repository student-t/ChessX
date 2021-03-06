/****************************************************************************
*   Copyright (C) 2015 by Jens Nissen jens-chessx@gmx.net                   *
****************************************************************************/

#ifndef MATCHPARAMETERDLG_H
#define MATCHPARAMETERDLG_H

#include "engineparameter.h"
#include <QDialog>

namespace Ui {
class MatchParameterDlg;
}

class MatchParameterDlg : public QDialog
{
    Q_OBJECT

public:
    explicit MatchParameterDlg(QWidget *parent = 0);
    ~MatchParameterDlg();

    static bool getParametersForEngineGame(EngineParameter &par);
    static bool getParametersForEngineMatch(EngineParameter &par);
    static bool getParametersForMatch(EngineParameter &par);

    typedef enum {
        EngineGame,
        EngineMatch,
        Match
    } Mode;

public slots:
    void SlotModeChanged(int);

private:
    static bool getParameters(EngineParameter &par, Mode mode);
    Ui::MatchParameterDlg *ui;
};

#endif // MATCHPARAMETERDLG_H
