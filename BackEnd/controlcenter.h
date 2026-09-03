#ifndef CONTROLCENTER_H
#define CONTROLCENTER_H

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QStringList>
#include <QVariantMap>

#include "BackEnd/socketmodeeditor.h"
#include "socketmodel.h"
#include "periphhandler.h"
#include "proghandle.h"
#include "progloader.h"
#include "linkstm.h"

class DeviceLogManager;
class FeatureUnlockController;
class JsonStorage;
class UiClickSound;

/**
 * @brief Управляющий класс бэкэнда, осуществляющий
 * композицию моделей данных и классов связи с железом
 */
class ControlCenter : public QObject
{
	Q_OBJECT
    Q_PROPERTY(QString debugOverlayText READ debugOverlayText NOTIFY debugOverlayTextChanged)
    Q_PROPERTY(bool debugUartEnabled READ debugUartEnabled WRITE setDebugUartEnabled NOTIFY debugUartEnabledChanged)
    Q_PROPERTY(int uartRate READ uartRate WRITE setUartRate NOTIFY uartRateChanged)
    Q_PROPERTY(bool cpuMonitorVisible READ cpuMonitorVisible WRITE setCpuMonitorVisible NOTIFY cpuMonitorVisibleChanged)
    Q_PROPERTY(bool argonDisabledByFault READ argonDisabledByFault NOTIFY argonDisabledByFaultChanged)
    Q_PROPERTY(int isnDacValue READ isnDacValue NOTIFY isnDacValueChanged)
    Q_PROPERTY(int isnAdcValue READ isnAdcValue NOTIFY isnAdcValueChanged)

public:
	explicit ControlCenter(QObject *parent = nullptr);
	~ControlCenter();

	/**
	 * @brief выполянет регистрацию класса в qml
	 */
	static void registerHandles();

	/**
	 * @brief возвращает указатель на модель сокетов, используемоую для
	 * отображения и редактирования текущих режимов
	 * @return
	 */
	QPointer<SocketModel> getSocketModel() const;

	/**
	 * @brief возвращает указатель на класс-редактор
	 * сокета для внемения изменений через QML
	 * @return
	 */
	QPointer<SocketModeEditor> getModeEditor() const;

	/**
	 * @brief инициализация - чтение предыдущих настроек, загрузка режимов из БД и т.д.
	 */
	void init();

	QPointer<ProgHandle> getHandle() const;

	/**
	 * @brief Запускает отложенное сохранение (с задержкой 2 секунды)
	 * Используется для частых изменений (мощность) чтобы не перегружать БД
	 */
	void scheduleSave();
	void flushPendingSave();

	/**
	 * @brief Устанавливает указатель на объект LinkStm для UART-коммуникации
	 * @param linkStm Указатель на объект LinkStm
	 */
	void setLinkStm(LinkStm* linkStm);
	void setDeviceLogManager(DeviceLogManager *deviceLog);
	void setJsonStorage(JsonStorage *jsonStorage);
	void setUiClickSound(UiClickSound *clickSound);
	void setFeatureUnlockController(class FeatureUnlockController *controller);

	QPointer<PeriphHandler> getPeripheryHandle() const;

	Q_INVOKABLE void cancelPowerOff();
	Q_INVOKABLE void confirmPowerOff();
    Q_INVOKABLE void shutdownSystemFromUi();
    Q_INVOKABLE void resetSystemFromUi();
	Q_INVOKABLE bool loadProgram(int progId, bool clear);
	Q_INVOKABLE QVariantMap localizedProgramTitle(int scopeId, int progId) const;
    Q_INVOKABLE void setNeutralResistPollEnabled(bool enabled);
    Q_INVOKABLE void setVolumeLevel(int level);
    Q_INVOKABLE void setLedOutput(int out, int color);
    /// Управление ИСН: enabled=false → 0,0; иначе voltage (0..110) и dacAction (IsnDacAction)
    Q_INVOKABLE void manageIsn(bool enabled, int voltage, int dacAction = 0);
    Q_INVOKABLE void appendDebugOverlayLine(const QString &line);
    Q_INVOKABLE void clearDebugOverlay();
    Q_INVOKABLE void stopActivation();
    Q_INVOKABLE void disableArgonModule();
    Q_INVOKABLE void applyStoredDeviceType();
    QString debugOverlayText() const;
    bool debugUartEnabled() const;
    void setDebugUartEnabled(bool enabled);
    int uartRate() const;
    void setUartRate(int rate);
    bool cpuMonitorVisible() const;
    bool argonDisabledByFault() const;
    void setCpuMonitorVisible(bool visible);
    int isnDacValue() const;
    int isnAdcValue() const;

signals:
    void debugOverlayTextChanged();
    void debugUartEnabledChanged();
    void uartRateChanged();
    void cpuMonitorVisibleChanged();
    void argonDisabledByFaultChanged();
    void deviceTypeApplied();
    void isnDacValueChanged();
    void isnAdcValueChanged();
    void powerOffConfirmationRequested(int timeoutSeconds);

public slots:
    void onPowerOffCommand();
    /// GPIO0_D5 LOW 100 мс: журнал P|7 и poweroff без диалога на экране.
    void onEmergencyPowerLoss();

private slots:
    void shutdownSystem();
    void resetSystem();

private:
	QSharedPointer<SocketModel> m_socketModel;

	QPointer<SocketModeEditor> m_editor;
	QPointer<ProgHandle> m_handle;
	QPointer<ProgLoader> m_progLoader;
	QPointer<PeriphHandler> m_periphery;
	FeatureUnlockController *m_featureUnlock = nullptr;
	QPointer<LinkStm> m_linkStm;
	QPointer<JsonStorage> m_jsonStorage;
	DeviceLogManager *m_deviceLog = nullptr;
	UiClickSound *m_uiClickSound = nullptr;
    bool m_systemMixerAtFull = false;
    int m_autoDelay = 0; // Задержка автозапуска в мс (runtime)
    bool m_powerOffConfirmationActive = false;
    bool m_powerOffRequested = false;
    bool m_shutdownStarted = false;
    QStringList m_debugOverlayLines;
    QString m_debugOverlayText;
    bool m_debugUartEnabled = false;
    int m_uartRate = 50;
    bool m_cpuMonitorVisible = false;
    bool m_argonDisabledByFault = false;
    int m_isnDacValue = -1;
    int m_isnAdcValue = -1;
    static constexpr int kDebugOverlayMaxLines = 40;

	QTimer* m_saveTimer = nullptr;  // Таймер для отложенного сохранения

private:
	/**
	 * @brief Обработчик входящих UART-данных
	 * @param rxData Указатель на принятую команду
	 */
	void makeHandleConnections();
	void initSocketsForPeriphery();
	void logPowerOff(quint8 reasonCode);

	void initSockets();
	void prepareConnectios();
	void ensureFullSystemMixerVolume();
	bool deviceTypeIsOnyxM() const;
	void sendStopArgonIfOnyxM();

	// void uartChat(LinkStm::UartRx* rxData);
	// void uartError(quint8 errorState);
};
#endif // CONTROLCENTER_H
