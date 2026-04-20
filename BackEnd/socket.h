#ifndef SOCKET_H
#define SOCKET_H

#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include <QSharedPointer>

#include "surgicalmode.h"
#include "Structures.h"
#include "halfsocket.h"
#include "pedal.h"

/**
 * @brief Класс описывающий один электрический сокет.
 * @details Содержит полный список присущих этому сокеты режимов,
 * поддерживает смену режима, настройку можности, выключение выключение режима
 * Предполагается взаимодействие с объектами типа SOCKET через модель
 */
class SOCKET
{

public:
    SOCKET(Onyx::SocType = Onyx::MONOPOLAR_1);
    SOCKET(const SOCKET& other);
    SOCKET& operator= (const SOCKET& other);

    /**
     * @brief Возвращает внутренний индекс текущего коаг режима
     * @return индекс
     */
    int coagModeIndex() const;

    /**
     * @brief Возвращает внутренний индекс текущего рез режима
     * @return индекс
     */
    int cutModeIndex() const;
    /**
     * @brief Возвращает ID текущего коаг режима
     * @return индекс
     */
    int coagModeId() const;

    /**
     * @brief Возвращает ID текущего рез режима
     * @return индекс
     */
    int cutModeId() const;
    /**
     * @brief Возвращает Num текущего коаг режима
     * @return индекс
     */
    int coagModeNum() const;

    /**
     * @brief Возвращает Num текущего рез режима
     * @return индекс
     */
    int cutModeNum() const;

    /**
     * @brief Возвращает тип сокета - МОНО1\2 Би1\2
     * @return тип
     */
    Onyx::SocType socketType() const;

    /**
     * @brief Возвращает статус сокета
     * @return тип
     */
    Onyx::SocStatus socketStatus() const;

    /**
     * @brief возвращает назание сокета
     * @return
     */
    const QString &socketName() const;

    /**
     * @brief возвращает название текущего режима коаг
     * @return
     */
    QString coagModeName() const;

    /**
     * @brief возвращает название текущего режима рез
     * @return
     */
    QString cutModeName() const;

    /**
     * @brief устанавливает новый относительный индекс режима коагуляции
     * @param newCoagModeIndex - устанавливаемый относительный индекс
     * @return true - индекс установлен
     * @return false - индекс не установлен
     */
    bool setCoagModeIndex(int newCoagModeIndex);

    /**
     * @brief устанавливает новый относительный индекс режима резания
     * @param newCutModeIndex - устанавливаемый относительный индекс
     * @return true - индекс установлен
     * @return false - индекс не установлен
     */
    bool setCutModeIndex(int newCutModeIndex);

    bool setModeId(int id, bool isCoag);

    /**
     * @brief установка типа сокеты
     * @param newSocketType
     */
    void setSocketType(Onyx::SocType newSocketType);

    /**
     * @brief смена текущего состояния сокета
     * @param newSocketStatus
     */
    void setSocketStatus(Onyx::SocStatus newSocketStatus);

    /**
     * @brief Установка локализированного имени сокета
     * @param newSocketName
     */
    void setSocketName(const QString &newSocketName);

    /**
     * @brief Геттер режима
     * @param modeIndex относительный индекс режима в этом сокете
     * @param isCoag выбор между режимами резания и коагуляции
     * @return Указатель на КОНСТАНТНЫЙ режим
     */
    CSurgModePtr getMode(int modeIndex, bool isCoag) const;

    /**
     * @brief Возращает текущую мощность текущего режима коагуляции
     * @return
     */
    int coagModePower() const;

    /**
     * @brief Устанавливает текущую мощность текущего режима коагуляции
     * @param newCoagModePower - задаваемая мощность
     * @return true - смена мощности режима выполнена успешно
     * @return false - изменение мощности не прошло
     */
    bool setCoagModePower(int newCoagModePower);

    /**
     * @brief Возращает текущую мощность текущего режима резания
     * @return
     */
    int cutModePower() const;

    /**
     * @brief Устанавливает текущую мощность текущего режима резания
     * @param newCutModePower - задаваемая мощность
     * @return true - смена мощности режима выполнена успешно
     * @return false - изменение мощности не прошло
     */
    bool setCutModePower(int newCutModePower);

    /**
     * @brief Задаеёт перечень достпных режимов резания и порядок их отображения
     * @param newCutModes - хэш новых режимов по их имени
     * @param order - порядок в котором список имён режимов будет выводиться пользователю
     * @todo Возможно, с учётом локализаций, необходимо использовать индекс из БД как ключ
     */
    void setCutModes(const QMap<int, SurgModePtr > &newCutModes,
                     const QStringList& order = {""});

    /**
     * @brief Задаеёт перечень достпных режимов коагуляции и порядок их отображения
     * @param newCoagModes - хэш новых режимов по их имени
     * @param order - порядок в котором список имён режимов будет выводиться пользователю
     * @todo Возможно, с учётом локализаций, необходимо использовать индекс из БД как ключ
     */
    void setCoagModes(const QMap<int, SurgModePtr > &newCoagModes,
                      const QStringList& order = {""});

    /**
     * @brief имена доступных режимов коагуляции
     * @return
     */
    QStringList coagModeNames() const;

    /**
     * @brief имена доступных режимов резания
     * @return
     */
    QStringList cutModeNames() const;

    /**
     * @brief айдишники доступных режимов коагуляции
     * @return
     */
    QStringList coagModeNamesIds() const;

    /**
     * @brief айдишники доступных режимов резания
     * @return
     */
    QStringList cutModeNamesIds() const;

    /**
     * @brief возвращает указатель на КОНСТАНТНЫЙ текущий режим коагуляции
     * @return
     */
    CSurgModePtr curCoagMode() const;

    /**
     * @brief возвращает указатель на КОНСТАНТНЫЙ текущий режим резания
     * @return
     */
    CSurgModePtr curCutMode() const;


    bool setInstrumIndex(int index, bool isCoag);
    bool setInstrumId(int id, bool isCoag);
    void setAllowed(bool allow);

    int displayMode() const;
    bool setDisplayMode(Onyx::SocDisplayMode newDisplayMode);

    int pedal() const;
    bool setPedal(int);

    QList<int> allowedPedals() const;

    Onyx::SocketState getInfo() const;

    int coagModeCount() const;
    int cutModeCount() const;

private:
    HalfSockPtr m_cutHalf = nullptr;
    HalfSockPtr m_coagHalf = nullptr;

    QList<int> m_allowedPedals = {};

    bool setModePower(int newPower, bool isCoag);
    bool setModeIndex(int index, bool isCoag);

    Onyx::SocType m_socketType;
    Onyx::SocStatus m_socketStatus;
    Onyx::SocDisplayMode m_displayMode;
    Pedal m_pedal;

    QString m_socketName;
};

using SockPtr = QSharedPointer<SOCKET>;

#endif // SOCKET_H
