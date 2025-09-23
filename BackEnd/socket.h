#ifndef SOCKET_H
#define SOCKET_H

#include <QObject>
#include <QString>
#include <QList>
#include <QStringList>
#include <QSharedPointer>

#include "surgicalmode.h"
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
    /*! Перечисление возможных типов сокета */
    enum SocType {  EMPTY, /*!<  Пустой сокет - заглушка на случай ошибок*/
                    BIPOLAR_1, /*!< Биполяр 1 */
                    BIPOLAR_2, /*!< Биполяр 2 */
                    MONOPOLAR_1, /*!< Монополяр 1 */
                    MONOPOLAR_2 /*!< Монополяр 2 */
                        };

    /*! Перечисление возможных состояний сокета */
    enum SocStatus {S_OFF, /*!< ОТКЛЮЧЕН */
                    S_DISABLED, /*!< Выключен, активация запрещена */
                    S_ENABLED, /*!< Включен, активация разрешена */
                    S_ACTIVE_COAG, /*!< Активирован, коагуляция */
                    S_ACTIVE_CUT, /*!< Активирован, резание */
                    S_ERROR /*!< Ошибка, активация запрещена */
                        }; 

    /*! Перечисление возможных отображений сокета */
    enum SocDisplayMode : int {   S_COLLAPSED = 0, /*!< свёрнут  */
                            S_EXPANDED, /*!< развёрнут */
                        };

    SOCKET(SOCKET::SocType = MONOPOLAR_1);

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
     * @brief Возвращает тип сокета - МОНО1\2 Би1\2
     * @return тип
     */
    SocType socketType() const;

    /**
     * @brief Возвращает статус сокета
     * @return тип
     */
    SocStatus socketStatus() const;

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
    void setSocketType(SOCKET::SocType newSocketType);

    /**
     * @brief смена текущего состояния сокета
     * @param newSocketStatus
     */
    void setSocketStatus(SocStatus newSocketStatus);

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

    /**
     * @brief формарование байт-массива текущих настроек сокета
     * @return
     */
    QByteArray toByteArray();

    bool setInstrumIndex(int index, bool isCoag);
    bool setInstrumId(int id, bool isCoag);
    void setAllowed(bool allow);

    int displayMode() const;
    void setDisplayMode(SocDisplayMode newDisplayMode);

    int pedal() const;
    void setPedal(int);

private:
    HalfSockPtr m_cutHalf = nullptr;
    HalfSockPtr m_coagHalf = nullptr;

    // CSurgModePtr getMode(const QString& name, bool isCoag) const;

    bool setModePower(int newPower, bool isCoag);
    bool setModeIndex(int index, bool isCoag);

    SocType m_socketType;
    SocStatus m_socketStatus;
    SocDisplayMode m_displayMode;
    Pedal m_pedal;

    QString m_socketName;
};

using SockPtr=QSharedPointer<SOCKET>;

#endif // SOCKET_H
