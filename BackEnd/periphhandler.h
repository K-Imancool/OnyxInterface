#ifndef PERIPHHANDLER_H
#define PERIPHHANDLER_H

#include "Structures.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QHash>
#include <QTimer>
#include <QVariantList>

class PeriphHandler : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool neutralElConnected READ neutralElConnected NOTIFY neutralElConnectedChanged)
	Q_PROPERTY(bool neutralElDivided READ neutralElDivided WRITE setNeutralElDivided NOTIFY neutralElDividedChanged)
	Q_PROPERTY(int neutralSize READ neutralSize WRITE setNeutralSize NOTIFY neutralSizeChanged)
	Q_PROPERTY(bool argonCylinder1Connected READ argonCylinder1Connected NOTIFY argonCylinder1ConnectedChanged)
	Q_PROPERTY(bool argonCylinder2Connected READ argonCylinder2Connected NOTIFY argonCylinder2ConnectedChanged)
	Q_PROPERTY(quint8 argonFlowRate READ argonFlowRate WRITE setArgonFlowRate NOTIFY sigArgonFlowRateChanged)
	Q_PROPERTY(quint8 argonRealRate READ argonRealRate NOTIFY argonRealRateChanged)
	Q_PROPERTY(bool argonBlowing READ argonBlowing NOTIFY argonBlowingChanged)
	Q_PROPERTY(bool activCylinderFirst READ activCylinderFirst WRITE setActivCylinderFirst NOTIFY activCylinderFirstChanged)
	Q_PROPERTY(bool enableActivation READ enableActivation WRITE setEnableActivation NOTIFY enableActivationChanged)
	Q_PROPERTY(bool activation READ activation NOTIFY activationChanged)
	Q_PROPERTY(bool activationStopWarningVisible READ activationStopWarningVisible NOTIFY activationStopWarningChanged)
	Q_PROPERTY(int activationStopWarningCode READ activationStopWarningCode NOTIFY activationStopWarningChanged)
    Q_PROPERTY(QVariantList activationStopWarningCodes READ activationStopWarningCodes NOTIFY activationStopWarningChanged)
    Q_PROPERTY(bool fullscreenErrorsEnabled READ fullscreenErrorsEnabled WRITE setFullscreenErrorsEnabled NOTIFY fullscreenErrorsEnabledChanged)
    Q_PROPERTY(int bi1AutoMode READ bi1AutoMode NOTIFY biAutoModeChanged)
    Q_PROPERTY(int bi2AutoMode READ bi2AutoMode NOTIFY biAutoModeChanged)
    Q_PROPERTY(int autoDelayMs READ autoDelayMs WRITE setAutoDelayMs NOTIFY autoDelayMsChanged)
    Q_PROPERTY(QString neutralResistText READ neutralResistText NOTIFY neutralResistTextChanged)
public:
	explicit PeriphHandler(QObject * parent = nullptr);

	/**
	 * @brief Возвращает состояние подключения нейтрального электрода
	 * @return true если НЭ подключён, false если нет
	 */
	bool neutralElConnected() const;

	/**
	 * @brief Возвращает тип нейтрального электрода
	 * @return true если разделённый, false если единый
	 */
	bool neutralElDivided() const;

	/**
	 * @brief Устанавливает тип нейтрального электрода
	 * @param divided true если разделённый, false если единый
	 */
	void setNeutralElDivided(bool divided);

	/**
	 * @brief Возвращает размер нейтрального электрода
	 * @return 0 = Small (< 5кг), 1 = Medium (5-15кг), 2 = Large (> 15кг)
	 */
	int neutralSize() const;

	/**
	 * @brief Устанавливает размер нейтрального электрода
	 * @param size 0 = Small, 1 = Medium, 2 = Large
	 */
	void setNeutralSize(int size);

	/**
	 * @brief Возвращает статус подключения баллона аргона №1
	 * @return true если баллон подключён, false если нет
	 */
	bool argonCylinder1Connected() const;

	/**
	 * @brief Возвращает статус подключения баллона аргона №2
	 * @return true если баллон подключён, false если нет
	 */
	bool argonCylinder2Connected() const;

	/**
	 * @brief Возвращает скорость потока аргона
	 * @return значение скорости потока аргона
	 */
	quint8 argonFlowRate() const;

	/**
	 * @brief Устанавливает скорость потока аргона
	 * @param rate значение скорости потока аргона
	 */
	void setArgonFlowRate(quint8 rate);

	/**
	 * @brief Возвращает реальную скорость потока аргона
	 * @return значение реальной скорости потока аргона
	 */
	quint8 argonRealRate() const;
	bool argonBlowing() const;

	/**
	 * @brief Возвращает активный баллон аргона
	 * @return true если активен первый баллон, false если второй
	 */
	bool activCylinderFirst() const;

	/**
	 * @brief Устанавливает активный баллон аргона
	 * @param first true для первого баллона, false для второго
	 */
	void setActivCylinderFirst(bool first);

	/**
	 * @brief Возвращает состояние разрешения активации
	 * @return true если активация разрешена, false если запрещена (открыты popup)
	 */
	bool enableActivation() const;

	// /**
	//  * @brief Устанавливает состояние разрешения активации
	//  * @param enable true для разрешения, false для запрета
	//  */
	void setEnableActivation(bool enable);

	// /**
	//  * @brief Возвращает состояние активации
	//  * @return true если активация выполняется, false если нет
	//  */
	bool activation() const;
	bool activationStopWarningVisible() const;
	int activationStopWarningCode() const;
    QVariantList activationStopWarningCodes() const;
    bool fullscreenErrorsEnabled() const;
    void setFullscreenErrorsEnabled(bool enabled);
    int bi1AutoMode() const;
    int bi2AutoMode() const;
    int autoDelayMs() const;
    QString neutralResistText() const;

	/**
	 * @brief Запускает продувку аргона
	 * Отправляет команду на выполнение продувки через UART
	 */
	Q_INVOKABLE void argonBlow();
	Q_INVOKABLE void clearActivationStopWarning();
	Q_INVOKABLE void dismissWarningCode(int warningCode);
    Q_INVOKABLE int biAutoMode(int socketId) const;
    Q_INVOKABLE void setBiAutoMode(int socketId, int mode);
    Q_INVOKABLE int autoMode(int socketId) const;
    Q_INVOKABLE void setAutoMode(int socketId, int mode);
    Q_INVOKABLE void setAutoDelayMs(int delayMs);

public slots:
	void unitStateHandler(Onyx::UnitState state);
	void setActivationActive(bool active);
	void setArgonRealRate(quint8 rate);
	void showActivationStopWarning(quint8 stopReason);
	void showWarningCode(quint8 warningCode);
	void onNeutralResistReceived(const QByteArray &data);

private:
	static QString formatNeutralResistData(const QByteArray &data);
	void checkArgonBlowMatch();
	void stopArgonBlow();

	bool m_argonCylinder1Connected;         // Подключение баллона 1
	bool m_argonCylinder2Connected;         // Баллона 2
	bool m_autoStStopTissue;                // Захвачена ткань в режиме АСС
	bool m_neutralElConnected;              // НЭ подключён
	bool m_neutralElDivided;                // НЭ разделённый
	int m_neutralSize;                      // Размер НЭ: 0 = Small, 1 = Medium, 2 = Large
	quint8 m_autoSSmode;                    // Режим AutoStop
	quint8 m_argonFlowRate;                 // Скорость потока аргона (установленная)
	quint8 m_argonRealRate;                 // Реальная скорость потока аргона
	bool m_argonBlowing;                    // Продувка аргона выполняется
	QTimer *m_blowMaxTimer;
	QTimer *m_blowMatchHoldTimer;
	bool m_activCylinderFirst;              // Активный баллон (true - первый, false - второй)
	quint8 m_wirelessPedalCharge;           // Заряд беспроводной педали
	bool m_enableActivation;                // Запрет активации (открыты popup)
    bool m_uiEnableActivation;             // Разрешение от UI (drawers/popups)
	bool m_activation;                      // Активация выполняется
	bool m_activationStopWarningVisible;
	int m_activationStopWarningCode;
    QList<int> m_activeWarningCodes;
    QHash<int, QTimer *> m_warningTimers;
    QHash<int, qint64> m_warningShownAtMs;
    QElapsedTimer m_warningClock;
    bool m_fullscreenErrorsEnabled = false;
    quint8 m_socketAutoModes[4];
    int m_autoDelayMs;
    QString m_neutralResistText;

    void recomputeEnableActivation();
    void clearWarningCode(int warningCode);
    void scheduleWarningClear(int warningCode);

signals:
	void neutralElConnectedChanged(bool connected);
	void neutralElDividedChanged(bool divided);
	void neutralSizeChanged(int size);
	void argonCylinder1ConnectedChanged(bool connected);
	void argonCylinder2ConnectedChanged(bool connected);
	void sigArgonFlowRateChanged(quint8 rate);
    void sigArgonBlow();
	void argonRealRateChanged(quint8 rate);
	void argonBlowingChanged(bool blowing);
	void activCylinderFirstChanged(bool first);
	void enableActivationChanged(bool enable);
	void activationChanged(bool active);
	void activationStopWarningChanged();
    void fullscreenErrorsEnabledChanged();
    void biAutoModeChanged();
    void autoModeChanged(int socketId, int mode);
    void autoDelayMsChanged(int delayMs);
    void neutralResistTextChanged();

};

#endif // PERIPHHANDLER_H
