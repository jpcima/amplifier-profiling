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
#include <QDebug>
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
    std::unique_ptr<cfloat[]> an_lo_response_;
    std::unique_ptr<cfloat[]> an_hi_response_;

    std::unique_ptr<double[]> an_lo_plot_mags_;
    std::unique_ptr<double[]> an_lo_plot_phases_;
    std::unique_ptr<double[]> an_hi_plot_mags_;
    std::unique_ptr<double[]> an_hi_plot_phases_;

    bool sweep_active_ = false;
    unsigned sweep_index_ = 0;
    int sweep_spl_ = Analysis::Signal_Lo;
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

    for (unsigned i = 0; i < ns; ++i) {
        const double lx1 = std::log10((double)Analysis::freq_range_min);
        const double lx2 = std::log10((double)Analysis::freq_range_max);
        double r = (double)i / (ns - 1);
        freqs[i] = std::pow(10.0, lx1 + r * (lx2 - lx1));
    }

    P->an_lo_response_.reset(new cfloat[ns]());
    P->an_hi_response_.reset(new cfloat[ns]());

    P->an_lo_plot_mags_.reset(new double[ns]());
    P->an_lo_plot_phases_.reset(new double[ns]());
    P->an_hi_plot_mags_.reset(new double[ns]());
    P->an_hi_plot_phases_.reset(new double[ns]());
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
        tr("Profile (*.profile)"));

    if (filename.isEmpty())
        return;

    QDir(filename).mkpath(".");

    cfloat *responses[] = {
        P->an_lo_response_.get(),
        P->an_hi_response_.get(),
    };
    const char *response_names[] = {
        "lo",
        "hi",
    };

    for (unsigned r = 0; r < 2; ++r) {
        std::ofstream file((filename + "/" + response_names[r] + ".dat").toLocal8Bit().data());
        file << std::scientific << std::setprecision(10);
        for (unsigned i = 0; i < Analysis::sweep_length; ++i) {
            double freq = P->an_freqs_[i];
            cfloat response = responses[r][i];
            file << freq << ' ' << std::abs(response) << ' ' << std::arg(response) << '\n';
        }
        if (!file.flush()) {
            QMessageBox::warning(P->mainwindow_, tr("Output error"), tr("Could not save profile data."));
            return;
        }
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
            int spl = msg->spl;
            P->an_freqs_[index] = msg->frequency;

            cfloat *response = ((spl == Analysis::Signal_Hi) ?
                                P->an_hi_response_ : P->an_lo_response_).get();

            response[index] = msg->response;

            double *plot_mags = ((spl == Analysis::Signal_Hi) ?
                                P->an_hi_plot_mags_ : P->an_lo_plot_mags_).get();
            double *plot_phases = ((spl == Analysis::Signal_Hi) ?
                                   P->an_hi_plot_phases_ : P->an_lo_plot_phases_).get();

            plot_mags[index] = 20 * std::log10(std::abs(msg->response));
            plot_phases[index] = std::arg(msg->response);

            ++index;
            if (index == Analysis::sweep_length) {
                index = 0;
                spl = !spl;
            }
            P->sweep_index_ = index;
            P->sweep_spl_ = spl;

            unsigned progress = P->sweep_progress_;
            ++progress;
            P->sweep_progress_ = std::min(progress, 2u * Analysis::sweep_length);
            P->mainwindow_->showProgress(progress * (0.5f / Analysis::sweep_length));

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
    msg.spl = P->sweep_spl_;

    proc.send_message(msg);

    P->mainwindow_->showCurrentFrequency(msg.frequency);
}

void Application::replotResponses()
{
    const unsigned ns = Analysis::sweep_length;
    P->mainwindow_->showPlotData
        (P->an_freqs_.get(), P->an_freqs_[P->sweep_index_],
         P->an_lo_plot_mags_.get(), P->an_lo_plot_phases_.get(),
         P->an_hi_plot_mags_.get(), P->an_hi_plot_phases_.get(),
         ns);
}
