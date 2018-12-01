//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "application.h"
#include "mainwindow.h"
#include "audioprocessor.h"
#include "analyzerdefs.h"
#include "messages.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <fstream>
#include <iomanip>
#include <complex>
#include <cmath>
typedef std::complex<float> cfloat;

struct Application::Impl {
    Audio_Processor *proc_ = nullptr;
    MainWindow *mainwindow_ = nullptr;
    QTimer *tm_rtupdates_ = nullptr;
    QTimer *tm_nextsweep_ = nullptr;

    std::unique_ptr<double[]> an_freqs_;
    std::unique_ptr<cfloat[]> an_response_;

    std::unique_ptr<double[]> an_plot_mags_;
    std::unique_ptr<double[]> an_plot_phases_;

    bool sweep_active_ = false;
    unsigned sweep_index_ = 0;
    unsigned sweep_progress_ = 0;
};

Application::Application(int &argc, char *argv[])
    : QApplication(argc, argv), P(new Impl)
{
    QTimer *tm;

    tm = P->tm_rtupdates_ = new QTimer(this);
    connect(tm, &QTimer::timeout, this, &Application::realtimeUpdateTick);
    tm->start(50);

    tm = P->tm_nextsweep_ = new QTimer(this);
    tm->setSingleShot(true);
    connect(tm, &QTimer::timeout, this, &Application::nextSweepTick);
}

Application::~Application()
{
}

void Application::setAudioProcessor(Audio_Processor &proc)
{
    P->proc_ = &proc;

    const unsigned ns = Analysis::sweep_length;
    double *freqs = new double[ns];
    P->an_freqs_.reset(freqs);
    P->an_response_.reset(new cfloat[ns]());

    P->an_plot_mags_.reset(new double[ns]());
    P->an_plot_phases_.reset(new double[ns]());

    for (unsigned i = 0; i < ns; ++i) {
        const double lx1 = std::log10((double)Analysis::freq_range_min);
        const double lx2 = std::log10((double)Analysis::freq_range_max);
        double r = (double)i / (ns - 1);
        freqs[i] = std::pow(10.0, lx1 + r * (lx2 - lx1));
    }
}

void Application::setMainWindow(MainWindow &win)
{
    P->mainwindow_ = &win;
}

void Application::setSweepActive(bool active)
{
    if (P->sweep_active_ == active)
        return;

    P->sweep_active_ = active;
    if (!active)
        P->tm_nextsweep_->stop();
    else {
        P->sweep_progress_ = 0;
        P->mainwindow_->showProgress(0);
        P->tm_nextsweep_->start(0);
    }
}

void Application::saveProfile()
{
    QString filename = QFileDialog::getSaveFileName(
        P->mainwindow_, tr("Save profile"),
        QString(),
        tr("Profile (*.dat)"));

    if (filename.isEmpty())
        return;

    std::ofstream file(filename.toLocal8Bit().data());
    file << std::scientific << std::setprecision(10);
    for (unsigned i = 0; i < Analysis::sweep_length; ++i) {
        double freq = P->an_freqs_[i];
        cfloat response = P->an_response_[i];
        file << freq << ' ' << std::abs(response) << ' ' << std::arg(response) << '\n';
    }

    if (!file.flush()) {
        QFile(filename).remove();
        QMessageBox::warning(P->mainwindow_, tr("Output error"), tr("Could not save profile data."));
    }
}

void Application::realtimeUpdateTick()
{
    Audio_Processor &proc = *P->proc_;

    while (Basic_Message *hmsg = proc.receive_message()) {
        switch (hmsg->tag) {
        case Message_Tag::NotifyFrequencyAnalysis: {
            auto *msg = (Messages::NotifyFrequencyAnalysis *)hmsg;

            unsigned index = P->sweep_index_;
            P->an_freqs_[index] = msg->frequency;
            P->an_response_[index] = msg->response;

            P->an_plot_mags_[index] = 20 * std::log10(std::abs(msg->response));
            P->an_plot_phases_[index] = std::arg(msg->response);

            ++index;
            P->sweep_index_ = (index < Analysis::sweep_length) ? index : 0;

            unsigned progress = P->sweep_progress_;
            ++progress;
            P->sweep_progress_ = std::min(progress, (unsigned)Analysis::sweep_length);
            P->mainwindow_->showProgress(progress * (1.0f / Analysis::sweep_length));

            replotResponses();

            if (P->sweep_active_)
                P->tm_nextsweep_->start(0);
            break;
        }
        default:
            assert(false);
            break;
        }
    }

    MainWindow &window = *P->mainwindow_;
    window.showLevels(proc.input_level(), proc.output_level());
}

void Application::nextSweepTick()
{
    Audio_Processor &proc = *P->proc_;
    unsigned index = P->sweep_index_;

    Messages::RequestAnalyzeFrequency msg;
    msg.frequency = P->an_freqs_[index];

    proc.send_message(msg);

    P->mainwindow_->showCurrentFrequency(msg.frequency);
}

void Application::replotResponses()
{
    const unsigned ns = Analysis::sweep_length;
    P->mainwindow_->showPlotData(P->an_freqs_.get(), P->an_freqs_[P->sweep_index_], P->an_plot_mags_.get(), P->an_plot_phases_.get(), ns);
}
