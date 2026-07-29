#include "controlcenter.h"
#include "DeviceLogManager.h"
#include "featureunlockcontroller.h"
#include "proghandle.h"

#include <QProcess>
#include <algorithm>
// #include <iostream>
#include <vector>

#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QDebug>
#include <QVector>
#include <QVariant>
#include <QElapsedTimer>

namespace {

bool runLoginAction(const QString &program, const QStringList &args, QString *errorMessage)
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(3000)) {
        if (errorMessage) {
            *errorMessage = proc.errorString();
        }
        return false;
    }

    proc.waitForFinished(10000);
    if (proc.state() == QProcess::Running) {
        return true;
    }

    if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
        return true;
    }

    if (errorMessage) {
        const QString stderrText = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        *errorMessage = stderrText.isEmpty()
                ? QStringLiteral("код выхода %1").arg(proc.exitCode())
                : stderrText;
    }
    return false;
}

} // namespace

std::vector<int> m_rolesSaveTriggered = {
    SocketModel::SocketRoles::CoagModeId,
    SocketModel::SocketRoles::CutModeId,
    SocketModel::SocketRoles::CoagModePower,
    SocketModel::SocketRoles::CutModePower,
    SocketModel::SocketRoles::CoagModeInstrNum,
    SocketModel::SocketRoles::CutModeInstrNum,
    SocketModel::SocketRoles::SocketPedal
};

ControlCenter::ControlCenter(QObject *parent)
    : QObject{parent},

    m_socketModel(new SocketModel()),
    m_editor(new SocketModeEditor(m_socketModel,this)),
    m_handle(new ProgHandle(this)),
    m_progLoader(new ProgLoader(this)),
    m_periphery(new PeriphHandler(this)),
    m_linkStm(nullptr),
    m_saveTimer(new QTimer(this))
{
	QQmlEngine::setObjectOwnership(m_socketModel.data(), QQmlEngine::CppOwnership);
	QQmlEngine::setObjectOwnership(m_editor, QQmlEngine::CppOwnership);
	QQmlEngine::setObjectOwnership(m_handle, QQmlEngine::CppOwnership);
	QQmlEngine::setObjectOwnership(m_periphery, QQmlEngine::CppOwnership);
	std::sort(m_rolesSaveTriggered.begin(), m_rolesSaveTriggered.end());
	init();
}

ControlCenter::~ControlCenter()
{ }

void ControlCenter::registerHandles()
{
	qmlRegisterUncreatableType<SocketModel>("BackEnd", 1, 0, "SocketModel", "should be one and exist not only for qml");
	qmlRegisterUncreatableType<SocketModeEditor>("BackEnd", 1, 0, "SocketModeEditor", "should be one and exist not only for qml");
	qmlRegisterUncreatableType<ProgHandle>("BackEnd", 1, 0, "ProgHandle", "should be one and exist not only for qml");
	qmlRegisterUncreatableType<PeriphHandler>("BackEnd", 1, 0, "PeriphHandle", "should be one and exist not only for qml");
	qmlRegisterUncreatableType<LinkStm>("BackEnd", 1, 0, "LinkStm", "LinkStm is created in C++");
}

QPointer<SocketModel> ControlCenter::getSocketModel() const
{
	return m_socketModel.data();
}

void ControlCenter::init()
{
	m_progLoader->setSocketModelPtr(m_socketModel);
	m_saveTimer->setSingleShot(true);
	m_saveTimer->setInterval(2000);  // 2 секунды
	m_socketModel->blockSignals(true);
	prepareConnectios();
	initSockets();
	m_socketModel->blockSignals(false);
}

void ControlCenter::makeHandleConnections()
{
	if (m_handle.isNull())
		return;

	connect(m_handle, &ProgHandle::signalRemoveSub,
	        m_socketModel.data(), &SocketModel::slotRemoveSubProg);

	connect(m_handle, &ProgHandle::signalRecomProgChosen,
	        this, [this](int progId, bool clear) {
//		qWarning() << "[ProgFlow] signalRecomProgChosen progId:" << progId << "clear:" << clear;
		m_progLoader->programmLoadSocketInit(progId, clear);
	});

	connect(m_progLoader, &ProgLoader::endoProgramAddBlocked,
	        m_handle, &ProgHandle::notifyEndoProgramMixRejected);

    connect(m_handle, &ProgHandle::signalFreeSettingsRequested,
            this, [this]() {
        m_progLoader->freeSettingsSocketInit(true);
    });

    connect(m_handle, &ProgHandle::signalEmptyFreeSettingsRequested,
            this, [this]() {
        m_progLoader->defaultSocketInit(true);
        m_progLoader->slotSaveCurrentState();
    });

    connect(m_handle, &ProgHandle::signalLastSettingsRequested,
            this, [this]() {
        if (!m_progLoader->loadCurrentState()) {
            m_progLoader->defaultSocketInit();
        }
    });

    connect(m_handle, &ProgHandle::signalSaveCurrentStateRequested,
            this, [this]() {
        if (!m_progLoader.isNull()) {
            m_progLoader->slotSaveCurrentState();
        }
    });
    
    connect(m_handle, &ProgHandle::signalDeleteAllUserProgs,
            this, [this]() {
        m_progLoader->deleteAllUserProgs();
        emit m_handle->updateScopes(false);
    });
    
	connect(m_handle, &ProgHandle::signalScopeRequest,
	        this, [this] (int id) {
//		qDebug() << "[ProgFlow] signalScopeRequest scopeId:" << id;
		m_handle->setProgList(m_progLoader->getProgs(id));
		m_handle->setProgSubLists(m_progLoader->lastProgSubLists());
	});

	connect(m_handle, &ProgHandle::updateScopes,
	        this, [this] (bool isRecom) {
//		qDebug() << "[ProgFlow] updateScopes isRecom:" << isRecom;
		m_progLoader->setCurLoaderType(isRecom ? ProgLoader::ptRecom : ProgLoader::ptUser);
		const auto categories = m_progLoader->getCategories();
//		qDebug() << "[ProgFlow] updateScopes categories:" << categories.size();
		m_handle->setScopeList(categories);
		// Списки программ подставляет signalScopeRequest → setProgList(getProgs(scopeId))
		// из setScopeIdx(0) внутри setScopeList. Вызов getProgs(-1) давал пустой запрос
		// Scope_ID = -1 и затирал m_progs до пустого — клик по списку не вызывал загрузку.
//		qDebug() << "[ProgFlow] updateScopes prog list size after setScopeList:"
//		         << m_handle->progNameList().size();
	});

	connect(m_handle, &ProgHandle::signalAddEmptyDefault,
	        m_progLoader, &ProgLoader::defaultSocketInit);

	connect(m_handle, &ProgHandle::signalSaveName,
	        this, [this](const QString& scopeName, const QString& progName) {
		m_progLoader->saveUserProg(scopeName, progName);
	});

	connect(m_handle, &ProgHandle::userProgExistsRequested,
	        this, [this](const QString& scopeName, const QString& progName, bool* result) {
		if (result) {
			*result = m_progLoader->userProgExists(scopeName, progName);
		}
	}, Qt::DirectConnection);

	connect(m_handle, &ProgHandle::signalCopyCurrent,
	        m_socketModel.data(), &SocketModel::copyCurrentList);

	connect(m_handle, &ProgHandle::signalDeleteProg,
	        this, [this](int progId) {
		m_progLoader->deleteUserProg(progId);
		emit m_handle->updateScopes(false);
	});

	connect(m_handle, &ProgHandle::signalDeleteScope,
	        this, [this](int scopeId) {
		m_progLoader->deleteUserScope(scopeId);
		emit m_handle->updateScopes(false);
	});

	connect(m_handle, &ProgHandle::signalRenameScope,
	        this, [this](int scopeId, const QString& name) {
		m_progLoader->renameUserScope(scopeId, name);
		emit m_handle->updateScopes(false);
	});

	connect(m_handle, &ProgHandle::signalRenameProg,
	        this, [this](int progId, const QString& name) {
		m_progLoader->renameUserProg(progId, name);
		emit m_handle->updateScopes(false);
	});

	// connect(m_)
}

QPointer<SocketModeEditor> ControlCenter::getModeEditor() const
{
	return m_editor;
}

void ControlCenter::initSockets()
{
	if (!m_progLoader->loadCurrentState())
		m_progLoader->defaultSocketInit();

	// m_handle->setScopeNameList(m_progLoader->getCategories());
}

void ControlCenter::prepareConnectios()
{
	makeHandleConnections();
}

QPointer<ProgHandle> ControlCenter::getHandle() const
{
	return m_handle;
}

void ControlCenter::scheduleSave()
{
	//переделал так - если уже бежит таймер, то пусть бежит. сохранится всё скопом
	//если не бежит - запустим
	// qDebug() << "scheduleSave";
	if (!m_saveTimer->isActive() || m_saveTimer->remainingTime() < 10) {
		m_saveTimer->stop();
		m_saveTimer->start();
	}
}

void ControlCenter::flushPendingSave()
{
	if (m_saveTimer && m_saveTimer->isActive()) {
		m_saveTimer->stop();
	}

	if (!m_progLoader.isNull()) {
		m_progLoader->slotSaveCurrentState();
	}
}

void ControlCenter::setDeviceLogManager(DeviceLogManager *deviceLog)
{
    m_deviceLog = deviceLog;
}

void ControlCenter::setJsonStorage(JsonStorage *jsonStorage)
{
	if (m_progLoader.isNull()) {
		return;
	}

	m_progLoader->setJsonStorage(jsonStorage);
	initSockets();
}

void ControlCenter::setFeatureUnlockController(FeatureUnlockController *controller)
{
	m_featureUnlock = controller;
	if (!m_progLoader.isNull()) {
		m_progLoader->setFeatureUnlockController(controller);
	}
	if (!controller) {
		return;
	}

	connect(controller, &FeatureUnlockController::activatedKeysChanged,
	        this, [this]() {
		initSockets();
		if (!m_handle.isNull()) {
			emit m_handle->updateScopes(m_handle->isRecomProgs());
		}
	});
}

void ControlCenter::setLinkStm(LinkStm* linkStm)
{
	if (m_linkStm == linkStm)
		return;

	// Отключаем старые соединения, если они были
	if (!m_linkStm.isNull()) {
		disconnect(m_linkStm, nullptr, nullptr, nullptr);
		m_linkStm->deleteLater();
		m_linkStm = nullptr;
	}

	m_linkStm = linkStm;

	// Подключаем обработчик входящих данных
	if (!m_linkStm.isNull()) {
		//connect(m_linkStm, &LinkStm::sigUnitStateChanged,
		//m_periphery, &PeriphHandler::unitStateHandler,
		//Qt::QueuedConnection);

		// Инициализируем текущие значения состояния в LinkStm
		m_linkStm->setEnableActivation(m_periphery->enableActivation());
		m_linkStm->setNeutralElDivided(m_periphery->neutralElDivided());
        m_linkStm->setArgonFlowRate(m_periphery->argonFlowRate());

		// Подключаем сигнал обновления данных сокетов
		// чуть громоздко но без лишних сигналов, полностью нативно
		connect(m_socketModel.data(), &SocketModel::dataChanged,
		        this, [this] (const QModelIndex &topLeft,
		        const QModelIndex &bottomRight,
		        const QVector<int> &/*roles = QVector<int>()*/) {
			int idxStart = topLeft.row();
			int idxStop = bottomRight.row();
			for (int i = idxStart; i <= idxStop; ++i) {
				//вызовы data по доке  reenterant так что мы можем предать в арги прям вызовы

				//НУЖНА ЗАЩИТА ОТ ВЫЗОВОВ ВО ВРЕМЯ ОБНОВЛЕНИЯ МОДЕЛИ
				QMetaObject::invokeMethod(  m_linkStm.data(),
				                            "updateSocketData",
				                            Qt::QueuedConnection,
				                            Q_ARG(int, i),  // socketIndex
				                            Q_ARG(quint16, topLeft.siblingAtRow(i).data(SocketModel::CutModeNum).value<quint16>()),
				                            Q_ARG(quint16, topLeft.siblingAtRow(i).data(SocketModel::CoagModeNum).value<quint16>()),
				                            Q_ARG(quint16, topLeft.siblingAtRow(i).data(SocketModel::CutModePower).value<quint16>()),
				                            Q_ARG(quint16, topLeft.siblingAtRow(i).data(SocketModel::CoagModePower).value<quint16>()),
				                            Q_ARG(quint8, topLeft.siblingAtRow(i).data(SocketModel::SocketPedal).value<quint8>()));
			}
		}, Qt::QueuedConnection);

		// Подключаем сигналы активации
		connect(m_linkStm, &LinkStm::sigStartActivation,
		        m_socketModel.data(), &SocketModel::startActivation,
		        Qt::QueuedConnection);
		connect(m_linkStm, &LinkStm::sigStartActivation,
		        m_periphery, [this](quint8, bool) {
			        m_periphery->setActivationActive(true);
		        },
		        Qt::QueuedConnection);
		connect(m_linkStm, &LinkStm::sigStopActivation,
		        m_socketModel.data(), &SocketModel::stopActivation,
		        Qt::QueuedConnection);
		connect(m_linkStm, &LinkStm::sigStopActivation,
		        m_periphery, [this](quint8) {
			        m_periphery->setActivationActive(false);
		        },
		        Qt::QueuedConnection);
		connect(m_linkStm, &LinkStm::sigPressed3rdKnob,
		        m_socketModel.data(),
		        [this](quint8 /*socketId*/) {
			        if (!m_socketModel) {
				        return;
			        }
			        const int n = m_socketModel->subProgCount();
			        if (n <= 1) {
				        return;
			        }
			        const int next = (m_socketModel->subProgIdx() + 1) % n;
			        m_socketModel->setSubProgIdx(next);
		        },
		        Qt::QueuedConnection);
        connect(m_linkStm, &LinkStm::sigStopActivation,
                m_periphery, &PeriphHandler::showWarningCode,
                Qt::QueuedConnection);
        connect(m_linkStm, &LinkStm::sigError,
                m_periphery, &PeriphHandler::showWarningCode,
                Qt::QueuedConnection);

		//-----------Коннекты получения перифейрийной информации
		connect(m_linkStm, &LinkStm::sigUnitStateChanged,
		        m_periphery, &PeriphHandler::unitStateHandler,
		        Qt::QueuedConnection);

		//-----------Коннекты передачи пользовательских действий
		connect(m_periphery, &PeriphHandler::activCylinderFirstChanged,
		        m_linkStm, &LinkStm::setActivCylinderFirst,
		        Qt::QueuedConnection);
		connect(m_periphery, &PeriphHandler::neutralElDividedChanged,
		        m_linkStm, &LinkStm::setNeutralElDivided,
		        Qt::QueuedConnection);
		connect(m_periphery, &PeriphHandler::enableActivationChanged,
		        m_linkStm,  &LinkStm::setEnableActivation,
		        Qt::QueuedConnection);
		connect(m_periphery, &PeriphHandler::sigArgonFlowRateChanged,
		        m_linkStm, &LinkStm::setArgonFlowRate,
		        Qt::QueuedConnection);
		connect(m_periphery, &PeriphHandler::sigArgonBlow,
		        m_linkStm, &LinkStm::argonBlow,
		        Qt::QueuedConnection);
		connect(m_linkStm, &LinkStm::sigNeutralResistReceived,
		        m_periphery, &PeriphHandler::onNeutralResistReceived,
		        Qt::QueuedConnection);
        connect(m_linkStm, &LinkStm::sigDebugOverlayLine,
                this, &ControlCenter::appendDebugOverlayLine,
                Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_linkStm.data(), "setDebugUart", Qt::QueuedConnection,
                                  Q_ARG(bool, m_debugUartEnabled));
        connect(m_periphery, &PeriphHandler::autoModeChanged,
                this, [this](int socketId, int mode) {
            if (m_linkStm.isNull() || m_periphery.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(m_linkStm.data(), "setSocketAutoMode", Qt::QueuedConnection,
                                      Q_ARG(int, socketId), Q_ARG(quint8, static_cast<quint8>(mode)));
        }, Qt::QueuedConnection);
        connect(m_periphery, &PeriphHandler::autoDelayMsChanged,
                this, [this](int delayMs) {
            m_autoDelay = delayMs;
        }, Qt::QueuedConnection);

        connect(m_linkStm, &LinkStm::sigPowerOffCommand,
                this, &ControlCenter::onPowerOffCommand,
                Qt::QueuedConnection);
        connect(m_linkStm, &LinkStm::sigReadyToPowerOffSent,
                this, &ControlCenter::shutdownSystem,
                Qt::QueuedConnection);

		// Инициализируем все сокеты текущими данными
		initSocketsForPeriphery();
        m_autoDelay = m_periphery->autoDelayMs();
        for (int socketId = 0; socketId < 4; ++socketId) {
            QMetaObject::invokeMethod(m_linkStm.data(), "setSocketAutoMode", Qt::QueuedConnection,
                                      Q_ARG(int, socketId),
                                      Q_ARG(quint8, static_cast<quint8>(m_periphery->autoMode(socketId))));
        }

		//qDebug() << "LinkStm connected to ControlCenter";
	}
}

void ControlCenter::initSocketsForPeriphery()
{
	if (m_linkStm.isNull() || !m_socketModel) {
		qWarning() << "Cannot initialize sockets in LinkStm: missing dependencies";
		return;
	}
	// Используем queued-вызовы, так как LinkStm работает в отдельном потоке
	for (int i = 0; i < m_socketModel->rowCount(QModelIndex()); ++i) {
		Onyx::SocketState info =
		        m_socketModel->index(i).data(SocketModel::SocketUartInfo).value<Onyx::SocketState>();

		QMetaObject::invokeMethod(
		            m_linkStm.data(),
		            "updateSocketData",
		            Qt::QueuedConnection,
		            Q_ARG(int, i),
		            Q_ARG(Onyx::SocketState, info));
	}
}

QPointer<PeriphHandler> ControlCenter::getPeripheryHandle() const
{
	return m_periphery;
}

void ControlCenter::logPowerOff(const QString &message)
{
    if (m_deviceLog) {
        m_deviceLog->logPowerOff(message);
    }
}

void ControlCenter::onPowerOffCommand()
{
    if (m_powerOffConfirmationActive || m_powerOffRequested) {
        return;
    }

    m_powerOffConfirmationActive = true;
    if (m_periphery) {
        m_periphery->setEnableActivation(false);
    }
    if (!m_linkStm.isNull()) {
        QMetaObject::invokeMethod(m_linkStm.data(), "requestReadyToPowerOff", Qt::QueuedConnection);
    }
    emit powerOffConfirmationRequested(10);
    logPowerOff(QStringLiteral("Получена команда PowerOff по UART LinkStm; ожидание подтверждения выключения"));
}

void ControlCenter::cancelPowerOff()
{
    if (!m_powerOffConfirmationActive || m_powerOffRequested) {
        return;
    }

    m_powerOffConfirmationActive = false;
    logPowerOff(QStringLiteral("Выключение аппарата отменено пользователем"));
}

void ControlCenter::confirmPowerOff()
{
    if (m_powerOffRequested) {
        return;
    }

    m_powerOffConfirmationActive = false;
    m_powerOffRequested = true;
    if (m_periphery) {
        m_periphery->setEnableActivation(false);
    }
    if (m_deviceLog) {
        m_deviceLog->finalizeSession();
    }
    logPowerOff(QStringLiteral("Выключение аппарата подтверждено; отправка ReadyToPowerOff по UART LinkStm"));

    if (!m_linkStm.isNull()) {
        QMetaObject::invokeMethod(m_linkStm.data(), "requestReadyToPowerOffWithData",
                                  Qt::QueuedConnection,
                                  Q_ARG(quint8, static_cast<quint8>(0x00)),
                                  Q_ARG(quint8, static_cast<quint8>(0x03)));
    }
}

void ControlCenter::shutdownSystemFromUi()
{
    if (m_shutdownStarted) {
        return;
    }

    m_powerOffConfirmationActive = false;
    m_powerOffRequested = true;
    if (m_periphery) {
        m_periphery->setEnableActivation(false);
    }
    if (m_deviceLog) {
        m_deviceLog->finalizeSession();
    }
    logPowerOff(QStringLiteral("poweroff через сервисное меню"));
    shutdownSystem();
}

void ControlCenter::resetSystemFromUi()
{
    if (m_shutdownStarted) {
        return;
    }

    m_powerOffConfirmationActive = false;
    m_powerOffRequested = true;
    if (m_periphery) {
        m_periphery->setEnableActivation(false);
    }
    if (m_deviceLog) {
        m_deviceLog->finalizeSession();
    }
    logPowerOff(QStringLiteral("reboot через сервисное меню"));
    resetSystem();
}

void ControlCenter::resetSystem()
{
    if (m_shutdownStarted) {
        return;
    }

    m_shutdownStarted = true;
    logPowerOff(QStringLiteral("Запуск системной команды перезагрузки roc-RK3566"));

    const QList<QPair<QString, QStringList>> rebootActions = {
        {QStringLiteral("systemctl"), {QStringLiteral("reboot")}},
        {QStringLiteral("shutdown"), {QStringLiteral("-r"), QStringLiteral("now")}},
        {QStringLiteral("reboot"), {}},
    };

    for (const auto &action : rebootActions) {
        QString error;
        if (runLoginAction(action.first, action.second, &error)) {
            return;
        }
        logPowerOff(QStringLiteral("reboot %1: %2").arg(action.first, error));
    }

    logPowerOff(QStringLiteral("Не удалось запустить системную команду перезагрузки"));
}

void ControlCenter::shutdownSystem()
{
    if (m_shutdownStarted) {
        return;
    }

    m_shutdownStarted = true;
    logPowerOff(QStringLiteral("Запуск системной команды выключения roc-RK3566"));

    if (QProcess::startDetached(QStringLiteral("systemctl"), QStringList{QStringLiteral("poweroff")})) {
        return;
    }
    if (QProcess::startDetached(QStringLiteral("shutdown"), QStringList{QStringLiteral("-h"), QStringLiteral("now")})) {
        return;
    }
    if (QProcess::startDetached(QStringLiteral("poweroff"), QStringList{})) {
        return;
    }

    logPowerOff(QStringLiteral("Не удалось запустить системную команду выключения"));
}

void ControlCenter::setNeutralResistPollEnabled(bool enabled)
{
    if (m_linkStm.isNull()) {
        return;
    }
    QMetaObject::invokeMethod(m_linkStm.data(), "setNeutralResistPollEnabled", Qt::QueuedConnection,
                              Q_ARG(bool, enabled));
}

void ControlCenter::setVolumeLevel(int level)
{
    if (m_linkStm.isNull()) {
        return;
    }
    QMetaObject::invokeMethod(m_linkStm.data(), "setVolume", Qt::QueuedConnection,
                              Q_ARG(int, level));
}

void ControlCenter::setLedOutput(int out, int color)
{
    if (m_linkStm.isNull()) {
        return;
    }
    auto *link = m_linkStm.data();
    const auto ledOut = static_cast<LinkStm::LedOutput>(out);
    const auto ledColor = static_cast<LinkStm::LedColor>(color);
    QMetaObject::invokeMethod(link, [link, ledOut, ledColor]() {
        link->setLedOutput(ledOut, ledColor);
    }, Qt::QueuedConnection);
}

bool ControlCenter::loadProgram(int progId, bool clear)
{
	if (m_progLoader.isNull())
		return false;
	return m_progLoader->programmLoadSocketInit(progId, clear);
}

QString ControlCenter::debugOverlayText() const
{
    return m_debugOverlayText;
}

bool ControlCenter::debugUartEnabled() const
{
    return m_debugUartEnabled;
}

void ControlCenter::setDebugUartEnabled(bool enabled)
{
    if (m_debugUartEnabled == enabled) {
        return;
    }
    m_debugUartEnabled = enabled;
    if (!m_linkStm.isNull()) {
        QMetaObject::invokeMethod(m_linkStm.data(), "setDebugUart", Qt::QueuedConnection,
                                  Q_ARG(bool, enabled));
    }
    if (!enabled) {
        clearDebugOverlay();
    }
    emit debugUartEnabledChanged();
}

int ControlCenter::uartRate() const
{
    return m_uartRate;
}

void ControlCenter::setUartRate(int rate)
{
    const int normalizedRate = rate == 500 ? 500 : 50;
    if (m_uartRate == normalizedRate) {
        return;
    }
    m_uartRate = normalizedRate;
    if (!m_linkStm.isNull()) {
        QMetaObject::invokeMethod(m_linkStm.data(), "setUartRate", Qt::QueuedConnection,
                                  Q_ARG(int, normalizedRate));
    }
    emit uartRateChanged();
}

bool ControlCenter::cpuMonitorVisible() const
{
    return m_cpuMonitorVisible;
}

void ControlCenter::setCpuMonitorVisible(bool visible)
{
    if (m_cpuMonitorVisible == visible) {
        return;
    }
    m_cpuMonitorVisible = visible;
    emit cpuMonitorVisibleChanged();
}

void ControlCenter::appendDebugOverlayLine(const QString &line)
{
    if (!m_debugUartEnabled || line.isEmpty()) {
        return;
    }

    m_debugOverlayLines.append(line);
    while (m_debugOverlayLines.size() > kDebugOverlayMaxLines) {
        m_debugOverlayLines.removeFirst();
    }

    const QString nextText = m_debugOverlayLines.join(QStringLiteral("\n"));
    if (nextText == m_debugOverlayText) {
        return;
    }
    m_debugOverlayText = nextText;
    emit debugOverlayTextChanged();
}

void ControlCenter::clearDebugOverlay()
{
    if (m_debugOverlayLines.isEmpty() && m_debugOverlayText.isEmpty()) {
        return;
    }
    m_debugOverlayLines.clear();
    m_debugOverlayText.clear();
    emit debugOverlayTextChanged();
}

//не понял задумки в этих фнкциях
// void ControlCenter::uartChat(LinkStm::UartRx* rxData)
// {
//     rxData = nullptr;
// }

// void ControlCenter::uartError(quint8 errorState)
// {
//     switch (errorState) {
//     case (LinkStm::STATE_OK + 32):
// // Все в порядке
// break;
//     case (LinkStm::STATE_TX_ERR + 32):
// // qWarning() << "UART TX Error";
// break;
//     case (LinkStm::STATE_NO_RX + 32):
// // qWarning() << "UART No RX";
// break;
//     case (LinkStm::STATE_RX_ERR + 32):
// // qWarning() << "UART RX Error";
// break;
//     case (LinkStm::STATE_RX_LEN_ERR + 32):
// // qWarning() << "UART RX Length Error";
// break;
//     case (LinkStm::STATE_RX_CRC_ERR + 32):
// // qWarning() << "UART RX CRC Error";
// break;
//     default:
// // qWarning() << "Some error: " << errorState;
// break;
//     }
// }

