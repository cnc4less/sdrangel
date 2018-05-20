///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "chanalyzernggui.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include <dsp/downchannelizer.h>
#include <QDockWidget>
#include <QMainWindow>

#include "dsp/threadedbasebandsamplesink.h"
#include "ui_chanalyzernggui.h"
#include "dsp/spectrumscopengcombovis.h"
#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "gui/glspectrum.h"
#include "gui/glscopeng.h"
#include "gui/basicchannelsettingsdialog.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "chanalyzerng.h"

ChannelAnalyzerNGGUI* ChannelAnalyzerNGGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    ChannelAnalyzerNGGUI* gui = new ChannelAnalyzerNGGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void ChannelAnalyzerNGGUI::destroy()
{
	delete this;
}

void ChannelAnalyzerNGGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString ChannelAnalyzerNGGUI::getName() const
{
	return objectName();
}

qint64 ChannelAnalyzerNGGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void ChannelAnalyzerNGGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	m_settings.m_frequency = m_channelMarker.getCenterFrequency();
	applySettings();
}

void ChannelAnalyzerNGGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

void ChannelAnalyzerNGGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_frequency);
    m_channelMarker.setBandwidth(m_settings.m_bandwidth * 2);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setLowCutoff(m_settings.m_lowCutoff);

    if (m_settings.m_ssb)
    {
        if (m_settings.m_bandwidth < 0) {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::usb);
        }
    }
    else
    {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->useRationalDownsampler->setChecked(m_settings.m_downSample);
    ui->channelSampleRate->setValue(m_settings.m_downSampleRate);
    setNewFinalRate();
    ui->BW->setValue(m_settings.m_bandwidth/100);
    ui->lowCut->setValue(m_settings.m_lowCutoff/100);
    ui->deltaFrequency->setValue(m_settings.m_frequency);
    ui->spanLog2->setCurrentIndex(m_settings.m_spanLog2);
    displayPLLSettings();
    ui->signalSelect->setCurrentIndex((int) m_settings.m_inputType);

    blockApplySettings(false);
}

void ChannelAnalyzerNGGUI::displayPLLSettings()
{
    if (m_settings.m_fll)
    {
        ui->pllPskOrder->setCurrentIndex(5);
    }
    else
    {
        int i = 0;
        for(; ((m_settings.m_pllPskOrder>>i) & 1) == 0; i++);
        ui->pllPskOrder->setCurrentIndex(i);
    }

    ui->pll->setChecked(m_settings.m_pll);
}

void ChannelAnalyzerNGGUI::setSpectrumDisplay()
{
    qDebug("ChannelAnalyzerNGGUI::setSpectrumDisplay: m_rate: %d", m_rate);
    if (m_settings.m_ssb)
    {
        ui->glSpectrum->setCenterFrequency(m_rate/4);
        ui->glSpectrum->setSampleRate(m_rate/2);
        ui->glSpectrum->setSsbSpectrum(true);
        ui->glSpectrum->setLsbDisplay(ui->BW->value() < 0);
    }
    else
    {
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(m_rate);
        ui->glSpectrum->setSsbSpectrum(false);
        ui->glSpectrum->setLsbDisplay(false);
    }
}

QByteArray ChannelAnalyzerNGGUI::serialize() const
{
    return m_settings.serialize();
}

bool ChannelAnalyzerNGGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true); // will have true
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        displaySettings();
        applySettings(true); // will have true
        return false;
    }
}

bool ChannelAnalyzerNGGUI::handleMessage(const Message& message)
{
    if (ChannelAnalyzerNG::MsgReportChannelSampleRateChanged::match(message))
    {
        qDebug() << "ChannelAnalyzerNGGUI::handleMessage: MsgReportChannelSampleRateChanged";
        ui->channelSampleRate->setValueRange(7, 2000U, m_channelAnalyzer->getInputSampleRate());
        ui->channelSampleRate->setValue(m_settings.m_downSampleRate);
        setNewFinalRate();

        return true;
    }

    return false;
}

void ChannelAnalyzerNGGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        qDebug("ChannelAnalyzerGUI::handleInputMessages: message: %s", message->getIdentifier());

        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void ChannelAnalyzerNGGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
	applySettings();
}

void ChannelAnalyzerNGGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ChannelAnalyzerNGGUI::tick()
{
	double powDb = CalcDb::dbPower(m_channelAnalyzer->getMagSq());
	m_channelPowerDbAvg(powDb);
	ui->channelPower->setText(tr("%1 dB").arg((Real) m_channelPowerDbAvg, 0, 'f', 1));

	if (m_channelAnalyzer->isPllLocked()) {
	    ui->pll->setStyleSheet("QToolButton { background-color : green; }");
	} else {
	    ui->pll->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
	}

	if (ui->pll->isChecked())
	{
		int freq = (m_channelAnalyzer->getPllFrequency() * m_channelAnalyzer->getChannelSampleRate()) / (2.0*M_PI);
		ui->pll->setToolTip(tr("PLL lock. Freq = %1 Hz").arg(freq));
	}
}

void ChannelAnalyzerNGGUI::on_channelSampleRate_changed(quint64 value)
{
    m_settings.m_downSampleRate = value;
    setNewFinalRate();
    applySettings();
}

void ChannelAnalyzerNGGUI::on_pll_toggled(bool checked)
{
	if (!checked) {
		ui->pll->setToolTip(tr("PLL lock"));
	}

	m_settings.m_pll = checked;
    applySettings();
}

void ChannelAnalyzerNGGUI::on_pllPskOrder_currentIndexChanged(int index)
{
    if (index < 5) {
        m_settings.m_pllPskOrder = (1<<index);
    }

    m_settings.m_fll = (index == 5);
    applySettings();
}

void ChannelAnalyzerNGGUI::on_useRationalDownsampler_toggled(bool checked)
{
    m_settings.m_downSample = checked;
    setNewFinalRate();
    applySettings();
}

int ChannelAnalyzerNGGUI::getRequestedChannelSampleRate()
{
    if (ui->useRationalDownsampler->isChecked()) {
        return ui->channelSampleRate->getValueNew();
    } else {
        return m_channelAnalyzer->getChannelizer()->getInputSampleRate();
    }
}

void ChannelAnalyzerNGGUI::on_signalSelect_currentIndexChanged(int index)
{
    m_settings.m_inputType = (ChannelAnalyzerNGSettings::InputType) index;
    applySettings();
}

void ChannelAnalyzerNGGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_frequency = m_channelMarker.getCenterFrequency();
    applySettings();
}

void ChannelAnalyzerNGGUI::on_BW_valueChanged(int value __attribute__((unused)))
{
    setFiltersUIBoundaries();
    m_settings.m_bandwidth = ui->BW->value() * 100;
    m_settings.m_lowCutoff = ui->lowCut->value() * 100;
	applySettings();
}

void ChannelAnalyzerNGGUI::on_lowCut_valueChanged(int value __attribute__((unused)))
{
	setFiltersUIBoundaries();
	m_settings.m_bandwidth = ui->BW->value() * 100;
	m_settings.m_lowCutoff = ui->lowCut->value() * 100;
	applySettings();
}

void ChannelAnalyzerNGGUI::on_spanLog2_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 6)) {
        return;
    }

    m_settings.m_spanLog2 = index;
    setNewFinalRate();
    applySettings();
}

void ChannelAnalyzerNGGUI::on_ssb_toggled(bool checked)
{
	m_settings.m_ssb = checked;
    setFiltersUIBoundaries();
    applySettings();
}

void ChannelAnalyzerNGGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
}

void ChannelAnalyzerNGGUI::onMenuDialogCalled(const QPoint& p)
{
    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.move(p);
    dialog.exec();

    m_settings.m_frequency = m_channelMarker.getCenterFrequency();
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();

    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);

    applySettings();
}

ChannelAnalyzerNGGUI::ChannelAnalyzerNGGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::ChannelAnalyzerNGGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true),
	m_rate(48000)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_spectrumVis = new SpectrumVis(SDR_RX_SCALEF, ui->glSpectrum);
	m_scopeVis = new ScopeVisNG(ui->glScope);
	m_spectrumScopeComboVis = new SpectrumScopeNGComboVis(m_spectrumVis, m_scopeVis);
	m_channelAnalyzer = (ChannelAnalyzerNG*) rxChannel; //new ChannelAnalyzerNG(m_deviceUISet->m_deviceSourceAPI);
	m_channelAnalyzer->setSampleSink(m_spectrumScopeComboVis);
	m_channelAnalyzer->setMessageQueueToGUI(getInputMessageQueue());

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

	ui->channelSampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
	ui->channelSampleRate->setValueRange(7, 2000U, 9999999U);

	ui->glSpectrum->setCenterFrequency(m_rate/2);
	ui->glSpectrum->setSampleRate(m_rate);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	ui->glSpectrum->setSsbSpectrum(false);
    ui->glSpectrum->setLsbDisplay(false);
	ui->BWLabel->setText("BP");

	ui->glSpectrum->connectTimer(MainWindow::getInstance()->getMasterTimer());
	ui->glScope->connectTimer(MainWindow::getInstance()->getMasterTimer());
	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::gray);
	m_channelMarker.setBandwidth(m_rate);
	m_channelMarker.setSidebands(ChannelMarker::usb);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setTitle("Channel Analyzer NG");
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only
	setTitleColor(m_channelMarker.getColor());

    m_deviceUISet->registerRxChannelInstance(ChannelAnalyzerNG::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);
	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

	m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setScopeGUI(ui->scopeGUI);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	displaySettings();
	applySettings(true);
}

ChannelAnalyzerNGGUI::~ChannelAnalyzerNGGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
	delete m_channelAnalyzer; // TODO: check this: when the GUI closes it has to delete the demodulator
	delete m_spectrumVis;
	delete m_scopeVis;
	delete m_spectrumScopeComboVis;
	delete ui;
}

void ChannelAnalyzerNGGUI::setNewFinalRate()
{
    m_rate = getRequestedChannelSampleRate() / (1<<m_settings.m_spanLog2);
    if (m_rate == 0) {
        m_rate = 48000;
    }
	qDebug("ChannelAnalyzerNGGUI::setNewFinalRate: %d m_spanLog2: %d", m_rate, m_settings.m_spanLog2);

	setFiltersUIBoundaries();

	QString s = QString::number(m_rate/1000.0, 'f', 1);
	ui->spanText->setText(tr("%1 kS/s").arg(s));

	ui->glScope->setSampleRate(m_rate);
	m_scopeVis->setSampleRate(m_rate);
}

void ChannelAnalyzerNGGUI::setFiltersUIBoundaries()
{
    bool dsb = !ui->ssb->isChecked();
    int bw = ui->BW->value();
    int lw = ui->lowCut->value();
    int bwMax = m_rate / 200;

    bw = bw < -bwMax ? -bwMax : bw > bwMax ? bwMax : bw;

    if (bw < 0) {
        lw = lw < bw+1 ? bw+1 : lw < 0 ? lw : 0;
    } else if (bw > 0) {
        lw = lw > bw-1 ? bw-1 : lw < 0 ? 0 : lw;
    } else {
        lw = 0;
    }

    if (dsb)
    {
        bw = bw < 0 ? -bw : bw;
        lw = 0;
    }

    QString bwStr = QString::number(bw/10.0, 'f', 1);
    QString lwStr = QString::number(lw/10.0, 'f', 1);

    if (dsb) {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(bwStr));
    } else {
        ui->BWText->setText(tr("%1k").arg(bwStr));
    }

    ui->lowCutText->setText(tr("%1k").arg(lwStr));

    ui->BW->blockSignals(true);
    ui->lowCut->blockSignals(true);

    ui->BW->setMaximum(bwMax);
    ui->BW->setMinimum(dsb ? 0 : -bwMax);
    ui->BW->setValue(bw);

    ui->lowCut->setMaximum(dsb ? 0 :  bw);
    ui->lowCut->setMinimum(dsb ? 0 : -bw);
    ui->lowCut->setValue(lw);

    ui->lowCut->blockSignals(false);
    ui->BW->blockSignals(false);

    setSpectrumDisplay();

    m_channelMarker.setBandwidth(bw * 200);
    m_channelMarker.setSidebands(dsb ? ChannelMarker::dsb : bw < 0 ? ChannelMarker::lsb : ChannelMarker::usb);

    if (!dsb) {
        m_channelMarker.setLowCutoff(lw * 100);
    }
}

void ChannelAnalyzerNGGUI::blockApplySettings(bool block)
{
    ui->glScope->blockSignals(block);
    ui->glSpectrum->blockSignals(block);
    m_doApplySettings = !block;
}

void ChannelAnalyzerNGGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        int sampleRate = getRequestedChannelSampleRate();

        ChannelAnalyzerNG::MsgConfigureChannelizer *msgChannelizer =
                ChannelAnalyzerNG::MsgConfigureChannelizer::create(sampleRate, m_channelMarker.getCenterFrequency());
        m_channelAnalyzer->getInputMessageQueue()->push(msgChannelizer);

        ChannelAnalyzerNG::MsgConfigureChannelizer *msg =
                ChannelAnalyzerNG::MsgConfigureChannelizer::create(sampleRate, m_channelMarker.getCenterFrequency());
        m_channelAnalyzer->getInputMessageQueue()->push(msg);

        ChannelAnalyzerNG::MsgConfigureChannelAnalyzer* message =
                ChannelAnalyzerNG::MsgConfigureChannelAnalyzer::create( m_settings, force);
        m_channelAnalyzer->getInputMessageQueue()->push(message);
	}
}

void ChannelAnalyzerNGGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void ChannelAnalyzerNGGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

