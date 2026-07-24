#include "periphhandler.h"

#include <QDebug>

PeriphHandler::PeriphHandler(QObject *parent)
    : QObject(parent),
    m_argonCylinder1Connected(false),
    m_argonCylinder2Connected(false),
    m_autoStStopTissue(false),
    m_neutralElConnected(false),
    m_neutralElDivided(true),
    m_neutralSize(2),  // По умолчанию Large (взрослый)
    m_autoSSmode(0),
    m_argonFlowRate(80),
    m_argonRealRate(0),
    m_argonBlowing(false),
    m_activCylinderFirst(true),  // По умолчанию активен первый баллон
    m_wirelessPedalCharge(0),
    m_enableActivation(true),
    m_uiEnableActivation(true),
    m_activation(false),
    m_activationStopWarningVisible(false),
    m_activationStopWarningCode(-1),
    m_socketAutoModes{0, 0, 0, 0},
    m_autoDelayMs(0),
    m_neutralResistText(QStringLiteral("—"))
{
    m_blowMaxTimer = new QTimer(this);
    m_blowMaxTimer->setSingleShot(true);
    m_blowMaxTimer->setInterval(5000);
    connect(m_blowMaxTimer, &QTimer::timeout, this, &PeriphHandler::stopArgonBlow);

    m_blowMatchHoldTimer = new QTimer(this);
    m_blowMatchHoldTimer->setSingleShot(true);
    m_blowMatchHoldTimer->setInterval(1000);
    connect(m_blowMatchHoldTimer, &QTimer::timeout, this, &PeriphHandler::stopArgonBlow);
}

void PeriphHandler::unitStateHandler(Onyx::UnitState state)
{
	if (m_neutralElConnected != state.neutraElConnected) {
		m_neutralElConnected = state.neutraElConnected;
	   emit neutralElConnectedChanged(m_neutralElConnected);
	}

	bool cylinder1Changed = false;
	bool cylinder2Changed = false;

	if (m_argonCylinder1Connected != state.argonCylinder1) {
		m_argonCylinder1Connected = state.argonCylinder1;
		cylinder1Changed = true;
		emit argonCylinder1ConnectedChanged(m_argonCylinder1Connected);
	}

	if (m_argonCylinder2Connected != state.argonCylinder2) {
		m_argonCylinder2Connected = state.argonCylinder2;
		cylinder2Changed = true;
		emit argonCylinder2ConnectedChanged(m_argonCylinder2Connected);
	}

	// Обновляем реальный расход аргона
	if (m_argonRealRate != state.argonRealRate) {
		m_argonRealRate = state.argonRealRate;
		// qDebug() << "РЕАЛЬНЫЙ РАСХОД: " << m_argonRealRate;
		emit argonRealRateChanged(m_argonRealRate);
	}
	if (m_argonBlowing) {
		checkArgonBlowMatch();
	}

	// Обновляем состояние активации на основе activOutput
	bool isActivating = (state.activOutput != 0);
	setActivationActive(isActivating);

	// Автоматическое переключение активного баллона
	if (cylinder1Changed || cylinder2Changed) {
		// Если второй баллон подключён, а первый нет - переключаемся на второй
		if (m_argonCylinder2Connected && !m_argonCylinder1Connected) {
			if (m_activCylinderFirst) {
				setActivCylinderFirst(false);
			}
		}
		// Если первый баллон подключён, а второй нет - переключаемся на первый
		else if (m_argonCylinder1Connected && !m_argonCylinder2Connected) {
			if (!m_activCylinderFirst) {
				setActivCylinderFirst(true);
			}
		}
	}
}

void PeriphHandler::setActivCylinderFirst(bool first)
{
	if (m_activCylinderFirst == first)
		return;

	m_activCylinderFirst = first;
	emit activCylinderFirstChanged(first);
}

bool PeriphHandler::argonCylinder1Connected() const
{
	return m_argonCylinder1Connected;
}

bool PeriphHandler::argonCylinder2Connected() const
{
	return m_argonCylinder2Connected;
}

quint8 PeriphHandler::argonFlowRate() const
{
	return m_argonFlowRate;
}

void PeriphHandler::setArgonFlowRate(quint8 rate)
{
	if (m_argonFlowRate == rate)
		return;

	m_argonFlowRate = rate;
    emit sigArgonFlowRateChanged(rate);
}

QString PeriphHandler::neutralResistText() const
{
    return m_neutralResistText;
}

QString PeriphHandler::formatNeutralResistData(const QByteArray &data)
{
    if (data.isEmpty()) {
        return QStringLiteral("—");
    }
    if (data.size() == 1) {
        return QString::number(static_cast<quint8>(data.at(0)));
    }
    if (data.size() == 2) {
        const quint16 value = (static_cast<quint8>(data.at(0)) << 8)
                | static_cast<quint8>(data.at(1));
        return QString::number(value);
    }
    QStringList parts;
    parts.reserve(data.size());
    for (int i = 0; i < data.size(); ++i) {
        parts << QString::number(static_cast<quint8>(data.at(i)));
    }
    return parts.join(QLatin1Char(' '));
}

void PeriphHandler::onNeutralResistReceived(const QByteArray &data)
{
    const QString formatted = formatNeutralResistData(data);
    if (m_neutralResistText == formatted) {
        return;
    }
    m_neutralResistText = formatted == "255" ?
                    QStringLiteral("—") :
                    (formatted + QStringLiteral(" \u03A9"));
    emit neutralResistTextChanged();
}

void PeriphHandler::argonBlow()
{
	if (m_argonBlowing) {
		return;
	}
	if (!m_argonCylinder1Connected && !m_argonCylinder2Connected) {
		qDebug() << "Argon blow ignored: no cylinder connected";
		return;
	}

	m_argonBlowing = true;
	emit argonBlowingChanged(m_argonBlowing);
	emit sigArgonBlow();

	m_blowMatchHoldTimer->stop();
	m_blowMaxTimer->start();
	qDebug() << "Argon blow command triggered";
}

bool PeriphHandler::argonBlowing() const
{
	return m_argonBlowing;
}

void PeriphHandler::checkArgonBlowMatch()
{
	if (!m_argonBlowing) {
		return;
	}

	if (m_argonRealRate == m_argonFlowRate) {
		if (!m_blowMatchHoldTimer->isActive()) {
			m_blowMatchHoldTimer->start();
		}
	} else {
		m_blowMatchHoldTimer->stop();
	}
}

void PeriphHandler::stopArgonBlow()
{
	if (!m_argonBlowing) {
		return;
	}

	m_argonBlowing = false;
	emit argonBlowingChanged(m_argonBlowing);
	m_blowMaxTimer->stop();
	m_blowMatchHoldTimer->stop();

	if (!m_activation && m_argonRealRate != 0) {
		m_argonRealRate = 0;
		emit argonRealRateChanged(m_argonRealRate);
	}
}

quint8 PeriphHandler::argonRealRate() const
{
	return m_argonRealRate;
}

bool PeriphHandler::activCylinderFirst() const
{
	return m_activCylinderFirst;
}

bool PeriphHandler::enableActivation() const
{
	return m_enableActivation;
}

void PeriphHandler::setEnableActivation(bool enable)
{
//	qDebug() << "setEnableActivation(bool enable)" << enable;
	if (m_uiEnableActivation == enable)
		return;

    m_uiEnableActivation = enable;
    recomputeEnableActivation();
}

bool PeriphHandler::activation() const
{
	return m_activation;
}

bool PeriphHandler::activationStopWarningVisible() const
{
    return m_activationStopWarningVisible;
}

int PeriphHandler::activationStopWarningCode() const
{
    return m_activationStopWarningCode;
}

QVariantList PeriphHandler::activationStopWarningCodes() const
{
    QVariantList result;
    result.reserve(m_activeWarningCodes.size());
    for (int code : m_activeWarningCodes) {
        result.append(code);
    }
    return result;
}

int PeriphHandler::bi1AutoMode() const
{
    return m_socketAutoModes[0];
}

int PeriphHandler::bi2AutoMode() const
{
    return m_socketAutoModes[1];
}

int PeriphHandler::autoDelayMs() const
{
    return m_autoDelayMs;
}

bool PeriphHandler::neutralElDivided() const
{
	return m_neutralElDivided;
}

void PeriphHandler::setNeutralElDivided(bool divided)
{
	if (m_neutralElDivided == divided)
		return;

	m_neutralElDivided = divided;
	emit neutralElDividedChanged(divided);
}

bool PeriphHandler::neutralElConnected() const
{
	return m_neutralElConnected;
}

int PeriphHandler::neutralSize() const
{
	return m_neutralSize;
}

void PeriphHandler::setNeutralSize(int size)
{
	if (m_neutralSize == size)
		return;

	m_neutralSize = size;
	emit neutralSizeChanged(size);
}

void PeriphHandler::setActivationActive(bool active)
{
	if (m_activation == active) {
		return;
	}

	if (active && m_argonBlowing) {
		m_argonBlowing = false;
		emit argonBlowingChanged(m_argonBlowing);
		m_blowMaxTimer->stop();
		m_blowMatchHoldTimer->stop();
	}

	m_activation = active;
	emit activationChanged(m_activation);

	if (!active && m_argonRealRate != 0 && !m_argonBlowing) {
		m_argonRealRate = 0;
		emit argonRealRateChanged(m_argonRealRate);
	}
}

void PeriphHandler::setArgonRealRate(quint8 rate)
{
	if (m_argonRealRate == rate)
		return;

	m_argonRealRate = rate;
	emit argonRealRateChanged(rate);
}

void PeriphHandler::showActivationStopWarning(quint8 stopReason)
{
    showWarningCode(stopReason);
}

void PeriphHandler::showWarningCode(quint8 warningCode)
{
    // 0x40 — штатная остановка активации без спец.причины, баннер не показываем.
    if (warningCode == 0x40) {
        return;
    }
    const int code = static_cast<int>(warningCode);
    bool changed = false;

    if (!m_activeWarningCodes.contains(code)) {
        m_activeWarningCodes.append(code);
        changed = true;
    }

    QTimer *timer = m_warningTimers.value(code, nullptr);
    if (!timer) {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, code]() {
            clearWarningCode(code);
        });
        m_warningTimers.insert(code, timer);
    }
    timer->start(5000);

    if (m_activeWarningCodes.isEmpty()) {
        m_activationStopWarningVisible = false;
        m_activationStopWarningCode = -1;
    } else {
        m_activationStopWarningVisible = true;
        m_activationStopWarningCode = m_activeWarningCodes.first();
    }

    if (changed) {
        recomputeEnableActivation();
        emit activationStopWarningChanged();
    }
}

void PeriphHandler::clearActivationStopWarning()
{
    if (m_activeWarningCodes.isEmpty() && !m_activationStopWarningVisible && m_activationStopWarningCode < 0) {
        return;
    }
    for (QTimer *timer : m_warningTimers) {
        if (timer) {
            timer->stop();
        }
    }
    m_activeWarningCodes.clear();
    m_activationStopWarningVisible = false;
    m_activationStopWarningCode = -1;
    recomputeEnableActivation();
    emit activationStopWarningChanged();
}

void PeriphHandler::recomputeEnableActivation()
{
    const bool hasWarnings = !m_activeWarningCodes.isEmpty();
    const bool effectiveEnable = m_uiEnableActivation && !hasWarnings;
    if (m_enableActivation == effectiveEnable) {
        return;
    }
    m_enableActivation = effectiveEnable;
    emit enableActivationChanged(m_enableActivation);
}

void PeriphHandler::clearWarningCode(int warningCode)
{
    const int index = m_activeWarningCodes.indexOf(warningCode);
    if (index < 0) {
        return;
    }
    m_activeWarningCodes.removeAt(index);
    if (m_activeWarningCodes.isEmpty()) {
        m_activationStopWarningVisible = false;
        m_activationStopWarningCode = -1;
    } else {
        m_activationStopWarningVisible = true;
        m_activationStopWarningCode = m_activeWarningCodes.first();
    }
    recomputeEnableActivation();
    emit activationStopWarningChanged();
}

int PeriphHandler::biAutoMode(int socketId) const
{
    if (socketId < 0 || socketId > 1) {
        return 0;
    }
    return m_socketAutoModes[socketId];
}

void PeriphHandler::setBiAutoMode(int socketId, int mode)
{
    if (socketId < 0 || socketId > 1) {
        return;
    }
    setAutoMode(socketId, mode);
}

int PeriphHandler::autoMode(int socketId) const
{
    if (socketId < 0 || socketId > 3) {
        return 0;
    }
    return m_socketAutoModes[socketId];
}

void PeriphHandler::setAutoMode(int socketId, int mode)
{
    if (socketId < 0 || socketId > 3) {
        return;
    }
    int bounded = mode;
    if (bounded < 0) {
        bounded = 0;
    } else if (bounded > 2) {
        bounded = 2;
    }
    const quint8 clamped = static_cast<quint8>(bounded);
    if (m_socketAutoModes[socketId] == clamped) {
        return;
    }
    m_socketAutoModes[socketId] = clamped;
    emit autoModeChanged(socketId, static_cast<int>(clamped));

    if (socketId <= 1 && clamped == 2) {
        const int otherBiSocket = socketId == 0 ? 1 : 0;
        if (m_socketAutoModes[otherBiSocket] == 2) {
            m_socketAutoModes[otherBiSocket] = 0;
            emit autoModeChanged(otherBiSocket, 0);
        }
    }

    if (socketId <= 1) {
        emit biAutoModeChanged();
    }
}

void PeriphHandler::setAutoDelayMs(int delayMs)
{
    int normalized = delayMs;
    if (normalized < 0) {
        normalized = 0;
    } else if (normalized > 1500) {
        normalized = 1500;
    }
    if (normalized == 0 || normalized == 500 || normalized == 1000 || normalized == 1500) {
        // valid values
    } else {
        normalized = 0;
    }

    if (m_autoDelayMs == normalized) {
        return;
    }
    m_autoDelayMs = normalized;
    emit autoDelayMsChanged(m_autoDelayMs);
}
