//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "application.h"
#include "analyzerdefs.h"
#include <qwt_scale_engine.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_grid.h>
#include <cmath>

struct MainWindow::Impl {
    Ui::MainWindow ui;
    QwtPlotCurve *curve_mag_ = nullptr;
    QwtPlotCurve *curve_phase_ = nullptr;
    QwtPlotMarker *marker_mag_ = nullptr;
    QwtPlotMarker *marker_phase_ = nullptr;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), P(new Impl)
{
    P->ui.setupUi(this);

    for (QwtPlot *plt : {P->ui.pltAmplitude, P->ui.pltPhase}) {
        plt->setAxisScale(QwtPlot::xBottom, Analysis::freq_range_min, Analysis::freq_range_max);
        plt->setAxisScaleEngine(QwtPlot::xBottom, new QwtLogScaleEngine);
        plt->setCanvasBackground(Qt::darkBlue);
        QwtPlotGrid *grid = new QwtPlotGrid;
        grid->setPen(Qt::gray, 0.0, Qt::DotLine);
        grid->attach(plt);
    }

    QwtPlotCurve *curve_mag = P->curve_mag_ = new QwtPlotCurve;
    curve_mag->attach(P->ui.pltAmplitude);
    curve_mag->setPen(Qt::green, 0.0, Qt::SolidLine);
    QwtPlotCurve *curve_phase = P->curve_phase_ = new QwtPlotCurve;
    curve_phase->attach(P->ui.pltPhase);
    curve_phase->setPen(Qt::green, 0.0, Qt::SolidLine);

    QwtPlotMarker *marker_mag = P->marker_mag_ = new QwtPlotMarker;
    marker_mag->attach(P->ui.pltAmplitude);
    marker_mag->setLineStyle(QwtPlotMarker::VLine);
    marker_mag->setLinePen(Qt::yellow, 0.0, Qt::DashLine);
    QwtPlotMarker *marker_phase = P->marker_phase_ = new QwtPlotMarker;
    marker_phase->attach(P->ui.pltPhase);
    marker_phase->setLineStyle(QwtPlotMarker::VLine);
    marker_phase->setLinePen(Qt::yellow, 0.0, Qt::DashLine);

    P->ui.pltAmplitude->setAxisScale(QwtPlot::yLeft, Analysis::db_range_min, Analysis::db_range_max);
    P->ui.pltPhase->setAxisScale(QwtPlot::yLeft, -M_PI, +M_PI);

    connect(P->ui.btn_startSweep, &QAbstractButton::clicked, theApplication, &Application::setSweepActive);
    connect(P->ui.btn_save, &QAbstractButton::clicked, theApplication, &Application::saveProfile);
}

MainWindow::~MainWindow()
{
}

void MainWindow::showCurrentFrequency(float f)
{
    QString text;
    if (f < 1000)
        text = QString::number(std::lround(f)) + " Hz";
    else
        text = QString::number(std::lround(f * 1e-3)) + " kHz";
    P->ui.lbl_frequency->setText(text);
}

void MainWindow::showLevels(float in, float out)
{
    float g_min = P->ui.vu_input->minimum();
    float a_min = std::pow(10.0f, g_min * 0.05f);
    float g_in = (in > a_min) ? (20 * std::log10(in)) : g_min;
    float g_out = (out > a_min) ? (20 * std::log10(out)) : g_min;
    P->ui.vu_input->setValue(g_in);
    P->ui.vu_output->setValue(g_out);
}

void MainWindow::showProgress(float progress)
{
    P->ui.progressBar->setValue(std::lround(progress * 100));
}

void MainWindow::showPlotData(const double *freqs, double freqmark, const double *mags, const double *phases, unsigned n)
{
    QwtPlotCurve *curve_mag = P->curve_mag_;
    QwtPlotCurve *curve_phase = P->curve_phase_;
    curve_mag->setRawSamples(freqs, mags, n);
    curve_phase->setRawSamples(freqs, phases, n);
    QwtPlotMarker *marker_mag = P->marker_mag_;
    QwtPlotMarker *marker_phase = P->marker_phase_;
    marker_mag->setXValue(freqmark);
    marker_phase->setXValue(freqmark);
    P->ui.pltAmplitude->replot();
    P->ui.pltPhase->replot();
}
