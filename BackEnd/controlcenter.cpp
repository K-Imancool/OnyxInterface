#include "controlcenter.h"
#include "proghandle.h"

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
	        this, [this] (int progId, bool clear) {
		qDebug() << "[ControlCenter] signalRecomProgChosen received progId=" << progId
		         << " clear=" << clear;
		const bool ok = m_progLoader->programmLoadSocketInit(progId, clear);
		qDebug() << "[ControlCenter] programmLoadSocketInit result=" << ok;
	});

	connect(m_handle, &ProgHandle::signalScopeRequest,
	        this, [this] (int id) {
		qDebug() << "[ControlCenter] signalScopeRequest received scopeId=" << id;
		m_handle->setProgList(m_progLoader->getProgs(id));
	});

	connect(m_handle, &ProgHandle::updateScopes,
	        this, [this] (bool isRecom) {
		qDebug() << "[ControlCenter] updateScopes isRecom=" << isRecom;
		m_progLoader->setCurLoaderType(isRecom ? ProgLoader::ptRecom : ProgLoader::ptUser);
		m_handle->setScopeList(m_progLoader->getCategories());
		m_handle->setProgList(m_progLoader->getProgs(-1));
	});

	connect(m_handle, &ProgHandle::signalAddEmptyDefault,
	        m_progLoader, &ProgLoader::defaultSocketInit);

	connect(m_handle, &ProgHandle::signalSaveName,
	        m_progLoader, &ProgLoader::saveUserProg);

	connect(m_handle, &ProgHandle::signalCopyCurrent,
	        m_socketModel.data(), &SocketModel::copyCurrentList);

	connect(m_handle, &ProgHandle::signalDeleteProg,
	        m_progLoader, &ProgLoader::deleteUserProg);

	connect(m_handle, &ProgHandle::signalDeleteScope,
	        m_progLoader, &ProgLoader::deleteUserScope);

	connect(m_handle, &ProgHandle::signalRenameScope,
	        m_progLoader, &ProgLoader::renameUserScope);

	connect(m_handle, &ProgHandle::signalRenameProg,
	        m_progLoader, &ProgLoader::renameUserProg);

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
	if (m_progLoader && m_saveTimer) {
		connect(m_saveTimer, &QTimer::timeout,
		        m_progLoader, &ProgLoader::slotSaveCurrentState);
	}

	if (m_progLoader && m_editor) {
		connect(m_editor, &SocketModeEditor::editingFinished,
		        this, [this] (bool success) {
			if (success) {
				scheduleSave();
			}
		});
	}

	connect(m_socketModel.data(), &SocketModel::dataChanged,
	        this, [this] (const QModelIndex&, const QModelIndex&, const QVector<int>& roles) {
		if (roles.empty()) {
			scheduleSave();
			return;
		}
		size_t checkIdx = 0;
		std::vector<int> rolesSrtd = std::vector(roles.begin(), roles.end());
		std::sort(rolesSrtd.begin(), rolesSrtd.end());
		//стд вектор, чтобы не переживать о shared-контейнерах и утечках
		//сортировка для того, чтобы справиться за 1 проход;
		for (const auto& item : rolesSrtd) {
			for (size_t i = checkIdx; i < m_rolesSaveTriggered.size(); ++i) {
				if (m_rolesSaveTriggered[i] < item) {
					checkIdx++;
					continue;
				}
				break;
			}
			//паранойная доп проверка на размер массива
			if (checkIdx < m_rolesSaveTriggered.size()
			        && item == m_rolesSaveTriggered[checkIdx]) {
				//нашли хоть одну нужную роль - тикаем, запустив сохранение
				scheduleSave();
				return;
			}
		}
	});
	connect(m_socketModel.data(), &SocketModel::subProgCountChanged,
	        this, &ControlCenter::scheduleSave);
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
		connect(m_linkStm, &LinkStm::sigStopActivation,
		        m_socketModel.data(), &SocketModel::stopActivation,
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

        // Инициализируем все сокеты текущими данными
        initSocketsForPeriphery();
        m_autoDelay = m_periphery->autoDelayMs();
        for (int socketId = 0; socketId < 4; ++socketId) {
            QMetaObject::invokeMethod(m_linkStm.data(), "setSocketAutoMode", Qt::QueuedConnection,
                                      Q_ARG(int, socketId),
                                      Q_ARG(quint8, static_cast<quint8>(m_periphery->autoMode(socketId))));
        }
        
//        qDebug() << "LinkStm connected to ControlCenter";
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

